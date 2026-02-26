/*
 * SPDX-FileCopyrightText: 2026 ID8 Engineering AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>

#include "aws.h"
#include "modem.h"
#include "sensor.h"

#define SENSOR_POLL_INTERVAL_MS           1000
#define AWS_PUBLISH_DATA_INTERVAL_SECONDS 30
#define AWS_PROCESS_TIMEOUT_MS            100
#define SNTP_RETRY_DELAY_SECONDS          5

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static struct sensor_value temp;
static struct sensor_value hum;

static uint32_t aws_last_publish;

static int sync_time_with_sntp(void)
{
	struct sntp_time ts;
	struct timespec tspec;
	int ret;
	int retry_count = 0;

	for (;;) {
		retry_count++;
		ret = sntp_simple(CONFIG_NET_CONFIG_SNTP_INIT_SERVER,
				  CONFIG_NET_CONFIG_SNTP_INIT_TIMEOUT, &ts);
		if (ret == 0) {
			break;
		}

		LOG_DBG("SNTP sync failed (attempt: %d): %d", retry_count, ret);
		k_sleep(K_SECONDS(SNTP_RETRY_DELAY_SECONDS));
	}

	tspec.tv_sec = ts.seconds;
	tspec.tv_nsec = ((uint64_t)ts.fraction * NSEC_PER_SEC) >> 32;

	return sys_clock_settime(SYS_CLOCK_REALTIME, &tspec);
}

int main(void)
{
	int ret = modem_connect_and_wait(K_SECONDS(120), K_SECONDS(20));
	if (ret < 0) {
		LOG_ERR("Modem init failed: %d", ret);
		return ret;
	}

	LOG_INF("Synchronizing time with SNTP");
	ret = sync_time_with_sntp();
	if (ret < 0) {
		LOG_ERR("SNTP time sync failed: %d", ret);
		return ret;
	}

	const struct device *const dev = DEVICE_DT_GET_ONE(st_hts221);

	if (!device_is_ready(dev)) {
		LOG_ERR("sensor: device not ready");
		return -ENODEV;
	}
	ret = aws_init();
	if (ret < 0) {
		LOG_ERR("AWS init failed: %d", ret);
		return ret;
	}
	ret = aws_connect();
	if (ret < 0) {
		LOG_ERR("AWS connect failed: %d", ret);
		return ret;
	}

	while (1) {
		ret = aws_process(AWS_PROCESS_TIMEOUT_MS);
		if (ret < 0) {
			LOG_ERR("AWS process failed: %d", ret);
			return ret;
		}
		uint32_t now = k_uptime_seconds();
		if (now - aws_last_publish >= AWS_PUBLISH_DATA_INTERVAL_SECONDS) {
			ret = read_sensor_parameters(dev, &temp, &hum);
			int64_t temp_milli = sensor_value_to_milli(&temp);
			int64_t hum_milli = sensor_value_to_milli(&hum);

			LOG_INF("Temperature: %" PRId64 " mC", temp_milli);
			LOG_INF("Relative Humidity: %" PRId64 " m%%", hum_milli);
			ret = aws_publish((int32_t)temp_milli, (int32_t)hum_milli);

			if (ret < 0) {
				LOG_ERR("AWS publish failed: %d", ret);
			} else {
				aws_last_publish = now;
			}
		}
		k_sleep(K_MSEC(SENSOR_POLL_INTERVAL_MS));
	}
}
