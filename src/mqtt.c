/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */
#ifdef HAVE_CONFIG_H
#   include "shelldown-config.h"
#endif

#include <embedlog.h>
#include <errno.h>
#include <jansson.h>
#include <mosquitto.h>
#include <string.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "id-map.h"
#include "macros.h"


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */
extern volatile int g_run;
static struct mosquitto *g_mqtt;
id_map_t  topic_map;

#define json_object_get_or_error(m, v, r, k) \
	v = json_object_get(r, k); \
	if (v == NULL) \
		goto_print(error, ELW, "["m"] no "k" in json: %s", payload)


/* ==========================================================================
                ____ ___   ____ _ / /_ / /_   ____   __  __ / /_
               / __ `__ \ / __ `// __// __/  / __ \ / / / // __ \
              / / / / / // /_/ // /_ / /_   / /_/ // /_/ // /_/ /
             /_/ /_/ /_/ \__, / \__/ \__/  / .___/ \__,_//_.___/
                           /_/            /_/
   ==========================================================================
    Publishes bool as "on" or "off"
   ========================================================================== */
static void pub_bool
(
	char        *topic,       /* first part of topic (base + device id) */
	const char  *dtopic,      /* second part of topic, device specific data */
	int          val,         /* 0 - off, !0 - on */
	int          qos,         /* qos to send message with */
	int          retain       /* mqtt retain flag */
)
{
	char         payload[4];  /* on or off */
	int          ret;         /* ret code from mosquitto_publish */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	strcat(topic, dtopic);
	strcpy(payload, val ? "on" : "off");
	el_print(ELD, "mqtt-pub-bool: %s: %s", topic, payload);
	ret = mosquitto_publish(g_mqtt, NULL, topic,
			strlen(payload)+1, payload, qos, retain);
	if (ret)
		el_print(ELW, "error sending %s to %s, reason: %s", payload, topic,
				mosquitto_strerror(ret));
}


/* ==========================================================================
    Publishes string on topic, as is
   ========================================================================== */
static void pub_string
(
	char        *topic,      /* first part of topic (base + device id) */
	const char  *dtopic,     /* second part of topic, device specific data */
	const char  *payload,    /* payload to send */
	int          qos,        /* qos to send message with */
	int          retain      /* mqtt retain flag */
)
{
	int          ret;        /* ret code from mosquitto_publish */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	strcat(topic, dtopic);
	el_print(ELD, "mqtt-pub: %s: %s", topic, payload);
	ret = mosquitto_publish(g_mqtt, NULL, topic,
			strlen(payload)+1, payload, qos, retain);
	if (ret)
		el_print(ELW, "error sending %s to %s, reason: %s", payload, topic,
				mosquitto_strerror(ret));

}


/* ==========================================================================
    Converts double $num to string, and the publishes message on topic+dropic
   ========================================================================== */
static void pub_number
(
	char        *topic,      /* first part of topic (base + device id) */
	const char  *dtopic,     /* second part of topic, device specific data */
	double       num,        /* number to publish (as string) */
	int          qos,        /* qos to send message with */
	int          retain,     /* mqtt retain flag */
	int          precision   /* float number precision */
)
{
	char         payload[128]; /* data to send over mqtt */
	int          ret;          /* ret code from mosquitto_publish */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	snprintf(payload, sizeof(payload), "%.*f", precision, num);
	strcat(topic, dtopic);
	el_print(ELD, "mqtt-pub: %s: %s", topic, payload);
	ret = mosquitto_publish(g_mqtt, NULL, topic,
			strlen(payload)+1, payload, qos, retain);
	if (ret)
		el_print(ELW, "error sending %s to %s, reason: %s", payload, topic,
				mosquitto_strerror(ret));

}


/* ==========================================================================
          ____   __  __ / /_     ____/ /___  _   __ (_)_____ ___   _____
         / __ \ / / / // __ \   / __  // _ \| | / // // ___// _ \ / ___/
        / /_/ // /_/ // /_/ /  / /_/ //  __/| |/ // // /__ /  __/(__  )
       / .___/ \__,_//_.___/   \__,_/ \___/ |___//_/ \___/ \___//____/
      /_/
   ==========================================================================
    Prases shelly plus 1pm specific data and publishes it on old mqtt format
   ========================================================================== */
void mqtt_publish_shellyplus1pm
(
	char        *topic,      /* part of topic, editable */
	const char  *payload,    /* jsonrpc payload from shelly */
	int          qos,        /* qos to send message with */
	int          retain      /* mqtt retain flag */
)
{
	json_t      *root;       /* json message from shelly */
	json_t      *params;     /* params part of json object */
	json_t      *swtch;      /* switch component */
	json_t      *value;      /* json value for foreach loop */
	const char  *key;        /* json key for foreach loop */
	char        *topic_end;  /* pointer to the end of $topic */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	topic_end = strchr(topic, '\0');

	root = json_loads(payload, 0, NULL);
	if (root == NULL)
		goto_print(error, ELW, "[s1pm] invalid json received %s", payload);

	/* shelly plus pm1 has only one switch, so id will always be 0 */
	json_object_get_or_error("s1pm", params, root, "params");
	json_object_get_or_error("s1pm", swtch, params, "switch:0");
	json_object_foreach(swtch, key, value)
	{
		/* each pub function will append to $topic, so if we go
		 * mutliple times in loop, topic will get malformed, so we
		 * reset it's state before every call */
		*topic_end = '\0';

		if (strcmp(key, "apower") == cmp_equal)
			pub_number(topic, "relay/0/power", json_number_value(value),
					qos, retain, 2);

		else if (strcmp(key, "voltage") == cmp_equal)
			pub_number(topic, "relay/0/voltage", json_number_value(value),
					qos, retain, 2);

		else if (strcmp(key, "output") == cmp_equal)
			pub_bool(topic, "relay/0", json_boolean_value(value), qos, retain);

		else if (strcmp(key, "temperature") == cmp_equal)
		{
			json_t  *temp_c;  /* internal temp in °C */
			json_t  *temp_f;  /* internal temp in °F */
			double   temp;    /* temp for temp status calculation */
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


			json_object_get_or_error("s1pm", temp_c, value, "tC");
			json_object_get_or_error("s1pm", temp_f, value, "tF");

			pub_number(topic, "temperature", json_number_value(temp_c),
					qos, retain, 1);
			pub_number(topic, "temperature_f", json_number_value(temp_c),
					qos, retain, 1);

			temp = json_number_value(temp_c);
			if (temp > VHIGH_TEMP)
				pub_string(topic, "temperature_status", "Very High", 2, 1);
			else if (temp > HIGH_TEMP)
				pub_string(topic, "temperature_status", "High", 2, 1);
			else
				pub_string(topic, "temperature_status", "Normal", 2, 1);
		}

		else if ((strcmp(key, "id") & strcmp(key, "source") &
				strcmp(key, "timer_started_at") &
				strcmp(key, "timer_duration")) == cmp_equal)
			continue; /* ignore unusable fields */

		else
			el_print(ELN, "unkown key received: %s, please report "
					"bug for missing key, so it can be ignored or "
					"implemented", key);
	}


error:
	json_decref(root);
}


/* ==========================================================================
                     ____   _____ (_)_   __ ____ _ / /_ ___
                    / __ \ / ___// /| | / // __ `// __// _ \
                   / /_/ // /   / / | |/ // /_/ // /_ /  __/
                  / .___//_/   /_/  |___/ \__,_/ \__/ \___/
                 /_/
   ==========================================================================
    Called by mosquitto on connection response.
   ========================================================================== */
static void mqtt_on_connect
(
	struct mosquitto  *mqtt,      /* mqtt session */
	void              *userdata,  /* not used */
	int                result     /* connection result */
)
{
	const char *reasons[5] =
	{
		"connected with success",
		"refused: invalid protocol version",
		"refused: invalid identifier",
		"refused: broker unavailable",
		"reserved"
	};
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	(void)userdata;
	result = result > 4 ? 4 : result;

	if (result != 0)
	{
		el_print(ELE, "connection failed %s", reasons[result]);
		return;
	}

	el_print(ELN, "connected to the broker");
	el_print(ELN, "subscribing to shelly topics");

	id_map_foreach(topic_map)
	{
		char  topic[ID_MAP_MAX];
		int   mid;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		sprintf(topic, "%s/events/rpc", node->src);
		if (mosquitto_subscribe(mqtt, &mid, topic, 0))
			continue_perror(ELE, "mosquitto_subscribe(%s)", topic);
		el_print(ELN, "sent subscribe request for %s, mid: %d", topic, mid);
	}
}


/* ==========================================================================
    Called by mosquitto when we subscribe to topic.
   ========================================================================== */
static void mqtt_on_subscribe
(
	struct mosquitto  *mqtt,         /* mqtt session */
	void              *userdata,     /* not used */
	int                mid,          /* message id */
	int                qos_count,    /* not used */
	const int         *granted_qos   /* not used */
)
{
	unused(mqtt);
	unused(userdata);
	unused(qos_count);
	unused(granted_qos);

	el_print(ELN, "subscribed to topic mid: %d", mid);
}


/* ==========================================================================
    Called by mosquitto when we disconnect from broker
   ========================================================================== */
static void mqtt_on_disconnect
(
	struct mosquitto  *mqtt,      /* mqtt session */
	void              *userdata,  /* not used */
	int                rc         /* disconnect reason */
)
{
	(void)userdata;

	if (rc == 0)
	{
		/* called by us, it's fine */
		el_print(ELN, "mqtt disconnected with success");
		return;
	}

	/* unexpected disconnect, try to reconnect */
	while (mosquitto_reconnect(mqtt) != 0)
		el_print(ELC, "calling mosquitto_reconnect()"); /* STOP. GIVING. UP! */
}

/* ==========================================================================
    Called by mosquitto when we receive message. We send here proper command
    to proper module based on topic.
   ========================================================================== */
static void mqtt_on_message
(
	struct mosquitto                *mqtt,     /* mqtt session */
	void                            *userdata, /* not used */
	const struct mosquitto_message  *msg       /* received message */
)
{
	const char                      *dst;      /* whereto republish message */
	char                            *src;      /* who sent us a message */
	char                             rtopic[TOPIC_MAX]; /* received topic */
	char                             topic[TOPIC_MAX]; /* topic to publish msg on */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	unused(mqtt);
	unused(userdata);


	if (strlen(msg->payload) != (size_t)msg->payloadlen)
		return_noval_print(ELW, "got invalid json message: %s on %s",
				msg->payload, msg->topic);

	el_print(ELD, "mqtt-msg: %s: %s", msg->topic, msg->payload);

	rtopic[sizeof(rtopic) - 1] = '\0';
	strncpy(rtopic, msg->topic, sizeof(rtopic));
	if (rtopic[sizeof(rtopic) - 1] != '\0')
		return_noval_print(ELW, "received topic too long: %s", msg->topic);

	/* for events topic will be in format
	 *   shellyplus1pm-7c87ce65bd9c/events/rpc
	 * we want to get that first part of it:
	 *   shellyplus1pm-7c87ce65bd9c */
	src = rtopic;
	while (*src != '/') src++;
	/* overwrite first '/' to get proper src device */
	*src = '\0';
	/* move back src pointer to the beginning */
	src = rtopic;

	/* we'll be constructing new destination topic now, so for
	 *   shellyplus1pm-7c87ce65bd9c/events/rpc
	 * and payload (truncated)
	 *   switch:0":{"id":0,"apower":0,"output":false,"source":"WS_in","voltage":0}
	 * assuming that shell id is mapped to heat/office
	 * we will be creating multiple message with topics and payload
	 * shellies/heat/office/relay/0/power 0
	 * shellies/heat/office/relay/0/voltage 0
	 * shellies/heat/office/relay/0 off
	 *
	 * if src can't be found in database, it will be used instead.
	 *
	 * That's an example, but gives a general idea what we will be
	 * doing here now */


	/* construct first part of topic:
	 *   shellies/heat/office
	 *     -- or --
	 *   shellies/shellyplus1pm-7c87ce65bd9c (if dst was not found in map) */
	dst = id_map_find(topic_map, src);
	sprintf(topic, "%s%s/", config->topic_base, dst ? dst : src);


	/* src served it's purpose, new we will reuse to get model id */
	while (*src != '-') src++;
	*src = '\0';
	src = rtopic;


#define publish_for_device(d) \
	if (strcmp(#d, src) == cmp_equal) { \
		mqtt_publish_##d(topic, msg->payload, msg->qos, msg->retain); \
		return; \
	}

	publish_for_device(shellyplus1pm);

#undef publish_for_device
}


/* ==========================================================================
                       __     __ _          ____
        ____   __  __ / /_   / /(_)_____   / __/__  __ ____   _____ _____
       / __ \ / / / // __ \ / // // ___/  / /_ / / / // __ \ / ___// ___/
      / /_/ // /_/ // /_/ // // // /__   / __// /_/ // / / // /__ (__  )
     / .___/ \__,_//_.___//_//_/ \___/  /_/   \__,_//_/ /_/ \___//____/
    /_/
   ==========================================================================
    Initializes mosquitto context and connects to broker at 'ip:port'
   ========================================================================== */
int mqtt_init
(
	const char  *ip,    /* ip of the broker to connect */
	int          port   /* port on which broker listens */
)
{
	int          n;     /* number of conn failures */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if (id_map_add_from_file(&topic_map, config->id_map_file))
		return_print(-1, errno, ELF, "Failed to load id map");

	id_map_print(topic_map);

	mosquitto_lib_init();

	if ((g_mqtt = mosquitto_new(NULL, 1, NULL)) == NULL)
		goto_perror(mosquitto_new_error, ELF, "mosquitto_new(1, NULL)");

	mosquitto_connect_callback_set(g_mqtt, mqtt_on_connect);
	mosquitto_message_callback_set(g_mqtt, mqtt_on_message);
	mosquitto_subscribe_callback_set(g_mqtt, mqtt_on_subscribe);
	mosquitto_disconnect_callback_set(g_mqtt, mqtt_on_disconnect);

	el_print(ELN, "connecting to %s:%d", ip, port);
	n = 60;
	for (;;)
	{
		if (mosquitto_connect(g_mqtt, ip, port, 60) == 0)
			/* connected to the broker, bail out of the loop */
			break;

		/* error connecting or interupted by signal */
		if (g_run == 0)
			/* someone lost his nerve and send us
			 * SIGTERM, enough is enough */
			goto_print(connect_error, ELW, "got SIGTERM while connecting");

		if (errno == ECONNREFUSED)
		{
			/* connection refused, either broker is not up yet
			 * or it actively rejets us, we will keep trying,
			 * printing log about it once a while */
			if (n++ == 60)
			{
				el_perror(ELW, "mosquitto_connect(%s, %d)", ip, port);
				n = 0;
			}

			/* sleep for some time before reconnecting */
			sleep(1);
			continue;
		}

		goto_perror(connect_error, ELF, "mosquitto_connect(%s, %d)", ip, port);
	}

	return 0;

connect_error:
	mosquitto_destroy(g_mqtt);
mosquitto_new_error:
	mosquitto_lib_cleanup();

	return -1;
}


/* ==========================================================================
    Starts thread that will loop mosquitto object until stopped.
   ========================================================================== */
int mqtt_loop_forever
(
	void
)
{
	return mosquitto_loop_forever(g_mqtt, -1, 1);
}


/* ==========================================================================
    Disconnect from broker and restroy mosquitto context.
   ========================================================================== */
int mqtt_cleanup
(
	void
)
{
	mosquitto_disconnect(g_mqtt);
	mosquitto_destroy(g_mqtt);
	mosquitto_lib_cleanup();
	return 0;
}


/* ==========================================================================
    Disconnects from broker, forcing mosquitto loop to return
   ========================================================================== */
void mqtt_stop
(
	void
)
{
	mosquitto_disconnect(g_mqtt);
}
