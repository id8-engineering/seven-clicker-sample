/*
 * SPDX-FileCopyrightText: 2026 ID8 Engineering AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/cellular.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <mender/client.h>
#include <mender/zephyr-image-update-module.h>

LOG_MODULE_REGISTER(mender_app, LOG_LEVEL_INF);

static char mender_identity_value[32];
static mender_identity_t mender_identity = {
	.name = "imei",
	.value = mender_identity_value,
};

static mender_err_t mender_network_connect_cb(void)
{
	/* The application keeps the PPP link up continuously. */
	return MENDER_OK;
}

static mender_err_t mender_network_release_cb(void)
{
	/* The PPP link is shared with AWS and should remain connected. */
	return MENDER_OK;
}

static mender_err_t mender_restart_cb(void)
{
	LOG_INF("Mender requested restart");
	sys_reboot(SYS_REBOOT_COLD);
	return MENDER_OK;
}

static mender_err_t mender_deployment_status_cb(mender_deployment_status_t status,
						const char *status_name)
{
	ARG_UNUSED(status);
	LOG_INF("Mender deployment status: %s", status_name);
	return MENDER_OK;
}

static mender_err_t mender_get_identity_cb(const mender_identity_t **identity)
{
	const struct device *modem = DEVICE_DT_GET(DT_ALIAS(modem));
	int ret;

	if (identity == NULL) {
		return MENDER_FAIL;
	}

	if (mender_identity.value[0] == '\0') {
		ret = cellular_get_modem_info(modem, CELLULAR_MODEM_INFO_IMEI, mender_identity_value,
					      sizeof(mender_identity_value));
		if (ret < 0 || mender_identity_value[0] == '\0') {
			LOG_ERR("Unable to read modem IMEI for Mender identity: %d", ret);
			return MENDER_FAIL;
		}
	}

	*identity = &mender_identity;
	return MENDER_OK;
}

int mender_start(void)
{
	mender_err_t ret;
	mender_client_config_t config = {
		.device_type = CONFIG_MENDER_DEVICE_TYPE,
		.host = NULL,
		.tenant_token = NULL,
		.device_tier = CONFIG_MENDER_DEVICE_TIER,
		.update_poll_interval = 0,
#ifndef CONFIG_MENDER_CLIENT_INVENTORY_DISABLE
		.inventory_update_interval = 0,
#endif
		.backoff_interval = 0,
		.max_backoff_interval = 0,
		.recommissioning = false,
	};
	mender_client_callbacks_t callbacks = {
		.network_connect = mender_network_connect_cb,
		.network_release = mender_network_release_cb,
		.deployment_status = mender_deployment_status_cb,
		.restart = mender_restart_cb,
		.get_identity = mender_get_identity_cb,
		.get_user_provided_keys = NULL,
	};

	ret = mender_client_init(&config, &callbacks);
	if (ret != MENDER_OK) {
		LOG_ERR("Mender init failed: %d", ret);
		return -EINVAL;
	}

	ret = mender_zephyr_image_register_update_module();
	if (ret != MENDER_OK) {
		LOG_ERR("Mender update module registration failed: %d", ret);
		return -EINVAL;
	}

	ret = mender_client_activate();
	if (ret != MENDER_OK) {
		LOG_ERR("Mender activation failed: %d", ret);
		return -EINVAL;
	}

	LOG_INF("Mender activated");
	return 0;
}
