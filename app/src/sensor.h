/*
 * SPDX-FileCopyrightText: 2026 ID8 Engineering AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENSORS_H_
#define _SENSORS_H_

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

int read_sensor_parameters(const struct device *dev, struct sensor_value *temp,
			   struct sensor_value *hum);

#endif /* _SENSORS_H_ */
