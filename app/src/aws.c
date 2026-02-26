/*
 * Copyright (c) 2023 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <zephyr/posix/netinet/in.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/posix/netdb.h>
#include <zephyr/posix/poll.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/drivers/cellular.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/data/json.h>
#include <zephyr/random/random.h>
#include <zephyr/logging/log.h>

#include "creds/creds.h"

#if defined(CONFIG_MBEDTLS_MEMORY_DEBUG)
#include <mbedtls/memory_buffer_alloc.h>
#endif

LOG_MODULE_REGISTER(aws, LOG_LEVEL_DBG);

#define AWS_BROKER_PORT CONFIG_AWS_MQTT_PORT

#define MQTT_BUFFER_SIZE 256u
#define APP_BUFFER_SIZE  4096u

#define MAX_RETRIES         10u
#define BACKOFF_EXP_BASE_MS 1000u
#define BACKOFF_EXP_MAX_MS  60000u
#define BACKOFF_CONST_MS    5000u
static struct sockaddr_in aws_broker;

static uint8_t rx_buffer[MQTT_BUFFER_SIZE];
static uint8_t tx_buffer[MQTT_BUFFER_SIZE];
static uint8_t buffer[APP_BUFFER_SIZE]; /* Shared between published and received messages */

static struct mqtt_client client_ctx;

static const char mqtt_client_name[] = CONFIG_AWS_THING_NAME;

#if (CONFIG_AWS_MQTT_PORT == 443 && !defined(CONFIG_MQTT_LIB_WEBSOCKET))
static const char *const alpn_list[] = {"x-amzn-mqtt-ca"};
#endif

#define TLS_TAG_DEVICE_CERTIFICATE 1
#define TLS_TAG_DEVICE_PRIVATE_KEY 1
#define TLS_TAG_AWS_CA_CERTIFICATE 2

static const sec_tag_t sec_tls_tags[] = {
	TLS_TAG_DEVICE_CERTIFICATE,
	TLS_TAG_AWS_CA_CERTIFICATE,
};

static int setup_credentials(void)
{
	int ret;

	ret = tls_credential_add(TLS_TAG_DEVICE_CERTIFICATE, TLS_CREDENTIAL_SERVER_CERTIFICATE,
				 public_cert, public_cert_len);
	if (ret < 0) {
		LOG_ERR("Failed to add device certificate: %d", ret);
		goto exit;
	}

	ret = tls_credential_add(TLS_TAG_DEVICE_PRIVATE_KEY, TLS_CREDENTIAL_PRIVATE_KEY,
				 private_key, private_key_len);
	if (ret < 0) {
		LOG_ERR("Failed to add CA certificate: %d", ret);
		goto exit;
	}

	ret = tls_credential_add(TLS_TAG_AWS_CA_CERTIFICATE, TLS_CREDENTIAL_CA_CERTIFICATE, ca_cert,
				 ca_cert_len);
	if (ret < 0) {
		LOG_ERR("Failed to add device private key: %d", ret);
		goto exit;
	}

exit:
	return ret;
}

static int publish_message(const char *topic, size_t topic_len, uint8_t *payload,
			   size_t payload_len)
{
	static uint32_t message_id = 1u;

	int ret;
	struct mqtt_publish_param msg;

	msg.retain_flag = 0u;
	msg.dup_flag = 0u;
	msg.message.topic.topic.utf8 = topic;
	msg.message.topic.topic.size = topic_len;
	msg.message.topic.qos = CONFIG_AWS_QOS;
	msg.message.payload.data = payload;
	msg.message.payload.len = payload_len;
	msg.message_id = message_id++;

	ret = mqtt_publish(&client_ctx, &msg);
	if (ret < 0) {
		LOG_ERR("Failed to publish message: %d", ret);
		return ret;
	}

	LOG_INF("PUBLISHED on topic \"%s\" [ id: %u qos: %u ], payload: %u B", topic,
		msg.message_id, msg.message.topic.qos, payload_len);
	LOG_HEXDUMP_DBG(payload, payload_len, "Published payload:");

	return ret;
}

static void mqtt_event_cb(struct mqtt_client *client, const struct mqtt_evt *evt)
{
	ARG_UNUSED(client);
	LOG_DBG("MQTT event: [%u] result: %d", evt->type, evt->result);

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result == 0) {
			LOG_INF("MQTT connected");
		} else {
			LOG_ERR("MQTT CONNACK failed: %d", evt->result);
		}
		break;

	case MQTT_EVT_PUBACK:
		break;

	case MQTT_EVT_DISCONNECT:
		break;

	case MQTT_EVT_PUBREC:
	case MQTT_EVT_PUBREL:
	case MQTT_EVT_PUBCOMP:
	case MQTT_EVT_PINGRESP:
	case MQTT_EVT_UNSUBACK:
	default:
		break;
	}
}

static void aws_client_setup(void)
{
	mqtt_client_init(&client_ctx);

	client_ctx.broker = &aws_broker;
	client_ctx.evt_cb = mqtt_event_cb;

	client_ctx.client_id.utf8 = (uint8_t *)mqtt_client_name;
	client_ctx.client_id.size = sizeof(mqtt_client_name) - 1;
	client_ctx.password = NULL;
	client_ctx.user_name = NULL;

	client_ctx.keepalive = CONFIG_MQTT_KEEPALIVE;

	client_ctx.protocol_version = MQTT_VERSION_3_1_1;

	client_ctx.rx_buf = rx_buffer;
	client_ctx.rx_buf_size = MQTT_BUFFER_SIZE;
	client_ctx.tx_buf = tx_buffer;
	client_ctx.tx_buf_size = MQTT_BUFFER_SIZE;

	/* setup TLS */
	client_ctx.transport.type = MQTT_TRANSPORT_SECURE;
	struct mqtt_sec_config *const tls_config = &client_ctx.transport.tls.config;

	tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
	tls_config->cipher_list = NULL;
	tls_config->sec_tag_list = sec_tls_tags;
	tls_config->sec_tag_count = ARRAY_SIZE(sec_tls_tags);
	tls_config->hostname = CONFIG_AWS_ENDPOINT;
	tls_config->cert_nocopy = TLS_CERT_NOCOPY_NONE;
#if (CONFIG_AWS_MQTT_PORT == 443 && !defined(CONFIG_MQTT_LIB_WEBSOCKET))
	tls_config->alpn_protocol_name_list = alpn_list;
	tls_config->alpn_protocol_name_count = ARRAY_SIZE(alpn_list);
#endif
}

struct backoff_context {
	uint16_t retries_count;
	uint16_t max_retries;

#if defined(CONFIG_AWS_EXPONENTIAL_BACKOFF)
	uint32_t attempt_max_backoff; /* ms */
	uint32_t max_backoff;         /* ms */
#endif
};

static void backoff_context_init(struct backoff_context *bo)
{
	__ASSERT_NO_MSG(bo != NULL);

	bo->retries_count = 0u;
	bo->max_retries = MAX_RETRIES;

#if defined(CONFIG_AWS_EXPONENTIAL_BACKOFF)
	bo->attempt_max_backoff = BACKOFF_EXP_BASE_MS;
	bo->max_backoff = BACKOFF_EXP_MAX_MS;
#endif
}

/* https://aws.amazon.com/blogs/architecture/exponential-backoff-and-jitter/ */
static void backoff_get_next(struct backoff_context *bo, uint32_t *next_backoff_ms)
{
	__ASSERT_NO_MSG(bo != NULL);
	__ASSERT_NO_MSG(next_backoff_ms != NULL);

#if defined(CONFIG_AWS_EXPONENTIAL_BACKOFF)
	if (bo->retries_count <= bo->max_retries) {
		*next_backoff_ms = sys_rand32_get() % (bo->attempt_max_backoff + 1u);

		/* Calculate max backoff for the next attempt (~ 2**attempt) */
		bo->attempt_max_backoff = MIN(bo->attempt_max_backoff * 2u, bo->max_backoff);
		bo->retries_count++;
	}
#else
	*next_backoff_ms = BACKOFF_CONST_MS;
	bo->retries_count++;
#endif
}

static int aws_client_try_connect(void)
{
	int ret;
	uint32_t backoff_ms;
	struct backoff_context bo;

	backoff_context_init(&bo);

	while (bo.retries_count <= bo.max_retries) {
		ret = mqtt_connect(&client_ctx);
		if (ret == 0) {
			goto exit;
		}

		backoff_get_next(&bo, &backoff_ms);

		LOG_ERR("Failed to connect: %d backoff delay: %u ms", ret, backoff_ms);
		k_msleep(backoff_ms);
	}

exit:
	return ret;
}

struct publish_payload {
	int32_t temperature;
	int32_t humidity;
};

static const struct json_obj_descr json_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct publish_payload, temperature, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct publish_payload, humidity, JSON_TOK_NUMBER),
};

static int publish(int32_t temperature, int32_t humidity)
{
	struct publish_payload pl = {
		.temperature = temperature,
		.humidity = humidity,
	};

	int len;
	memset(buffer, 0, sizeof(buffer));
	len = json_obj_encode_buf(json_descr, ARRAY_SIZE(json_descr), &pl, buffer, sizeof(buffer));
	if (len < 0) {
		LOG_ERR("Failed to encode JSON payload: %d", len);
		return len;
	}

	/* Use string length to avoid publishing trailing bytes if encoder behavior changes. */
	size_t payload_len = strnlen((char *)buffer, sizeof(buffer));
	LOG_INF("Publishing JSON: %.*s", (int)payload_len, (char *)buffer);

	return publish_message(CONFIG_AWS_PUBLISH_TOPIC, strlen(CONFIG_AWS_PUBLISH_TOPIC), buffer,
			       payload_len);
}

int aws_publish(int32_t temperature, int32_t humidity)
{
	return publish(temperature, humidity);
}

static int resolve_broker_addr(struct sockaddr_in *broker)
{
	int ret;
	struct addrinfo *ai = NULL;

	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
	};
	char port_string[6] = {0};

	snprintf(port_string, sizeof(port_string), "%d", AWS_BROKER_PORT);
	ret = getaddrinfo(CONFIG_AWS_ENDPOINT, port_string, &hints, &ai);
	if (ret == 0) {
		char addr_str[INET_ADDRSTRLEN];

		memcpy(broker, ai->ai_addr, MIN(ai->ai_addrlen, sizeof(struct sockaddr_storage)));

		inet_ntop(AF_INET, &broker->sin_addr, addr_str, sizeof(addr_str));
		LOG_INF("Resolved: %s:%u", addr_str, htons(broker->sin_port));
	} else {
		LOG_ERR("failed to resolve hostname err = %d (errno = %d)", ret, errno);
	}

	if (ai != NULL) {
		freeaddrinfo(ai);
	}

	return ret;
}

int aws_init(void)
{
	int ret;

	ret = setup_credentials();
	if (ret < 0) {
		return ret;
	}

	ret = resolve_broker_addr(&aws_broker);
	if (ret < 0) {
		return ret;
	}

	aws_client_setup();

	return 0;
}

int aws_connect(void)
{
	int ret;

	ret = aws_client_try_connect();
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int aws_process(int timeout_ms)
{
	int ret;
	struct zsock_pollfd fds[] = {
		{
			.fd = client_ctx.transport.tls.sock,
			.events = ZSOCK_POLLIN,
		},
	};

	if (client_ctx.transport.type != MQTT_TRANSPORT_SECURE || fds[0].fd < 0) {
		return -ENOTCONN;
	}

	ret = zsock_poll(fds, ARRAY_SIZE(fds), timeout_ms);
	if (ret < 0) {
		return -errno;
	}

	if (ret > 0 && (fds[0].revents & ZSOCK_POLLIN)) {
		ret = mqtt_input(&client_ctx);
		if (ret < 0) {
			return ret;
		}
	}

	ret = mqtt_live(&client_ctx);
	if (ret < 0 && ret != -EAGAIN) {
		return ret;
	}

	return 0;
}
