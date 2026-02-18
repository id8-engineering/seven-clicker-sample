#include <stdint.h>

#ifndef APP_MQTT_CLIENT_H_
#define APP_MQTT_CLIENT_H_

int app_mqtt_init(void);
int app_mqtt_connect(void);
int app_mqtt_publish_temp_hum(int32_t temp_mdeg_c, int32_t hum_mpermille);
int app_mqtt_disconnect(void);

#endif /* APP_MQTT_CLIENT_H_ */
