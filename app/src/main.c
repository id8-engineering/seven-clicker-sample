/*
 * SPDX-FileCopyrightText: 2026 ID8 Engineering AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "modem.h"
#include "sensor.h"

#define SENSOR_POLL_INTERVAL_MS 2000

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static struct sensor_value temp;
static struct sensor_value hum;

int main(void)
{
	int ret = modem_connect_and_wait(K_SECONDS(120), K_SECONDS(20));
	if (ret != 0) {
		LOG_ERR("Modem init failed: %d", ret);
		return ret;
	}
	const struct device *const dev = DEVICE_DT_GET_ONE(st_hts221);

	if (!device_is_ready(dev)) {
		LOG_ERR("sensor: device not ready");
		return -ENODEV;
	}

	while (1) {
		if (read_sensor_parameters(dev, &temp, &hum) == 0) {
			int64_t temp_milli = sensor_value_to_milli(&temp);
			int64_t hum_milli = sensor_value_to_milli(&hum);

			LOG_INF("Temperature: %" PRId64 " mC", temp_milli);
			LOG_INF("Relative Humidity: %" PRId64 " m%%", hum_milli);
		}
		k_sleep(K_MSEC(SENSOR_POLL_INTERVAL_MS));
	}
}
