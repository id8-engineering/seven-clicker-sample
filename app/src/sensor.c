/*
 * SPDX-FileCopyrightText: 2026 ID8 Engineering AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "sensor.h"

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_INF);

int read_sensor_parameters(const struct device *dev, struct sensor_value *temp,
			   struct sensor_value *hum)
{
	int ret;

	if (dev == NULL || temp == NULL || hum == NULL) {
		LOG_ERR("Invalid argument: dev=%p temp=%p hum=%p", dev, temp, hum);
	}

	ret = sensor_sample_fetch(dev);
	if (ret < 0) {
		LOG_ERR("Sensor sample update error: %d", ret);
		return ret;
	}

	ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, temp);
	if (ret < 0) {
		LOG_ERR("Cannot read HTS221 temperature channel: %d", ret);
		return ret;
	}

	ret = sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, hum);
	if (ret < 0) {
		LOG_ERR("Cannot read HTS221 humidity channel: %d", ret);
		return ret;
	}
	return 0;
}
