/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef SHELLDOWN_MQTT_H
#define SHELLDOWN_MQTT_H 1

int mqtt_init(const char *ip, int port);
int mqtt_cleanup(void);
int mqtt_publish(const char *topic, const void *payload, int paylen);
void mqtt_stop(void);
int mqtt_loop_forever(void);

void mqtt_pub_string(const char *btopic, const char *topic, const char *payload,
		int qos, int retain);
void mqtt_pub_number(const char *btopic, const char *topic, double num,
		int qos, int retain, int precision);
void mqtt_pub_bool(const char *btopic, const char *topic, int val,
		int qos, int retain);

#endif
