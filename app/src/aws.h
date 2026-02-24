/*
 * SPDX-FileCopyrightText: 2026 ID8 Engineering AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AWS_H_
#define AWS_H_

#include <stdint.h>

int aws_init(void);
int aws_connect(void);
int aws_process(int timeout_ms);
int aws_publish(int32_t temperature, int32_t humidity);

#endif /* AWS_H_ */
