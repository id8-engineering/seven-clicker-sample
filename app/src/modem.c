/*
 * SPDX-FileCopyrightText: 2026 ID8 Engineering AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/drivers/cellular.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include "modem.h"

LOG_MODULE_REGISTER(modem, LOG_LEVEL_INF);

#define L4_CONNECTED BIT(0)
#define L4_DNS_ADDED BIT(1)
#define L4_EVENT_MASK                                                                              \
	(NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED | NET_EVENT_DNS_SERVER_ADD)

static struct k_event l4_event;

static void print_cellular_info(const struct device *modem)
{
	int rc;
	int16_t rssi;
	char buffer[64];

	if (!device_is_ready(modem)) {
		LOG_WRN("Modem device is not ready for info query");
		return;
	}

	rc = cellular_get_signal(modem, CELLULAR_SIGNAL_RSSI, &rssi);
	if (!rc) {
		LOG_INF("RSSI: %d", rssi);
	}

	rc = cellular_get_modem_info(modem, CELLULAR_MODEM_INFO_IMEI, &buffer[0], sizeof(buffer));
	if (!rc) {
		LOG_INF("IMEI: %s", buffer);
	}
	rc = cellular_get_modem_info(modem, CELLULAR_MODEM_INFO_MODEL_ID, &buffer[0],
				     sizeof(buffer));
	if (!rc) {
		LOG_INF("MODEL_ID: %s", buffer);
	}
	rc = cellular_get_modem_info(modem, CELLULAR_MODEM_INFO_MANUFACTURER, &buffer[0],
				     sizeof(buffer));
	if (!rc) {
		LOG_INF("MANUFACTURER: %s", buffer);
	}
	rc = cellular_get_modem_info(modem, CELLULAR_MODEM_INFO_SIM_IMSI, &buffer[0],
				     sizeof(buffer));
	if (!rc) {
		LOG_INF("SIM_IMSI: %s", buffer);
	}
	rc = cellular_get_modem_info(modem, CELLULAR_MODEM_INFO_SIM_ICCID, &buffer[0],
				     sizeof(buffer));
	if (!rc) {
		LOG_INF("SIM_ICCID: %s", buffer);
	}
	rc = cellular_get_modem_info(modem, CELLULAR_MODEM_INFO_FW_VERSION, &buffer[0],
				     sizeof(buffer));
	if (!rc) {
		LOG_INF("FW_VERSION: %s", buffer);
	}
}

/* Track L4 state transitions so modem_connect_and_wait() can block on events. */
static void l4_event_handler(uint64_t event, struct net_if *iface, void *info, size_t info_length,
			     void *user_data)
{
	ARG_UNUSED(iface);
	ARG_UNUSED(info);
	ARG_UNUSED(info_length);
	ARG_UNUSED(user_data);

	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		k_event_post(&l4_event, L4_CONNECTED);
		break;
	case NET_EVENT_DNS_SERVER_ADD:
		k_event_post(&l4_event, L4_DNS_ADDED);
		break;
	case NET_EVENT_L4_DISCONNECTED:
		k_event_set(&l4_event, 0);
		break;
	default:
		break;
	}
}

NET_MGMT_REGISTER_EVENT_HANDLER(l4_events, L4_EVENT_MASK, l4_event_handler, NULL);

int modem_connect_and_wait(k_timeout_t l4_timeout, k_timeout_t dns_timeout)
{
	const struct device *modem = DEVICE_DT_GET(DT_ALIAS(modem));
	struct net_if *ppp_iface;
	uint32_t events;

	/* Initialize once per connection attempt so waits see fresh state. */
	k_event_init(&l4_event);

	ppp_iface = net_if_get_first_by_type(&NET_L2_GET_NAME(PPP));
	if (ppp_iface == NULL) {
		LOG_ERR("PPP interface not found");
		return -ENODEV;
	}

	(void)net_if_up(ppp_iface);

	LOG_INF("Waiting for L4 connected");
	events = k_event_wait(&l4_event, L4_CONNECTED, false, l4_timeout);
	if ((events & L4_CONNECTED) == 0U) {
		LOG_ERR("L4 was not connected in time");
		return -ETIMEDOUT;
	}
	LOG_INF("L4 connected");
	print_cellular_info(modem);

	LOG_INF("Waiting for DNS server added");
	events = k_event_wait(&l4_event, L4_DNS_ADDED, false, dns_timeout);
	if ((events & L4_DNS_ADDED) == 0U) {
		LOG_ERR("DNS server was not added in time");
		return -ETIMEDOUT;
	}

	return 0;
}
