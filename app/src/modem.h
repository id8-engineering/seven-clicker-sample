/*
 * SPDX-FileCopyrightText: 2026 ID8 Engineering AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MODEM_H_
#define MODEM_H_

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * modem_connect_and_wait() brings up the cellular modem and PPP interface,
 * then waits for L4 connectivity. If dns_timeout is not K_NO_WAIT, it also
 * waits for DNS server announcement.
 */
int modem_connect_and_wait(k_timeout_t l4_timeout, k_timeout_t dns_timeout);

#ifdef __cplusplus
}
#endif

#endif /* MODEM_H_ */
