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
#include "shelly.h"


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
	int                mid;       /* mqtt sub message id */
	const char        *tbase;     /* base topic from config */
	const char        *reasons[5] =
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
	tbase = config->topic_base;

	if (result != 0)
	{
		el_print(ELE, "connection failed %s", reasons[result]);
		return;
	}

	el_print(ELN, "connected to the broker");
	el_print(ELN, "subscribing to shelly topics");

#if 0
	/* all v1 shelly devices lies in shellies/# mqtt namespace */
	if (config->republish)
		if (mosquitto_subscribe(mqtt, &mid, "shellies/#", 0))
			el_print(ELN, "sent subscribe request for shellies/#, mid: %d", mid);
#endif


	id_map_foreach(topic_map)
	{
		char  topic[ID_MAP_MAX];
		int   api_ver;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		/* simple macro, to subscribe to printf-like formatted topic,
		 * will also print error on failure */
#define subscribe(...) { \
			sprintf(topic, __VA_ARGS__); \
			if (mosquitto_subscribe(mqtt, &mid, topic, 0)) \
				continue_perror(ELE, "mosquitto_subscribe(%s)", topic); \
			el_print(ELN, "sent subscribe request for %s, mid: %d", topic, mid);}

		api_ver = shelly_id_to_ver(node->src);
		if (api_ver == -1) continue;

		if (api_ver ==  1)
		{
			subscribe("shellies/%s/#", node->src);
			if (strncmp(node->src, "shellyswitch25", 14) == cmp_equal)
			{
				subscribe("%s%s/roller/0/command", tbase, node->dst);
				subscribe("%s%s/roller/0/command/pos", tbase, node->dst);
			}
			if (strncmp(node->src, "shellyplug", 10) == cmp_equal)
				subscribe("%s%s/relay/0/command", tbase, node->dst);

			continue;
		}

		if (api_ver ==  2)
		{
			subscribe("%s/events/rpc", node->src);
			if (strncmp(node->src, "shellyplus1pm", 13) == cmp_equal)
				subscribe("%s%s/relay/0/command", tbase, node->dst);
		}
#undef subscribe
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
    Called by mqtt_on_message when user calls /command
   ========================================================================== */
static void mqtt_on_message_cmd
(
	struct mosquitto                *mqtt,     /* mqtt session */
	void                            *userdata, /* not used */
	const struct mosquitto_message  *msg       /* received message */
)
{
	(void)userdata;
	char                            *t;        /* ptr to somewhere in rtopic */
	char                            *src;      /* command topic */
	char                            *cmd;      /* command to execute */
	char                            *id;       /* id of shelly subdev (number)*/
	char                            *shelly_id;/* shelly id from topic */
	int                              ret;      /* return from mosquitto_pub */
	char                             rtopic[TOPIC_MAX]; /* received topic */
	char                             topic[TOPIC_MAX]; /* topic to publish msg*/
	char                             cmd_json[1024]; /* json to send as cmd */
	id_map_t                         node;     /* topic id node */
	int                              api_ver;  /* shelly api version */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	rtopic[sizeof(rtopic) - 1] = '\0';
	strncpy(rtopic, msg->topic, sizeof(rtopic));
	if (rtopic[sizeof(rtopic) - 1] != '\0')
		return_noval_print(ELW, "received topic too long: %s", msg->topic);

	src = rtopic;
	/* skip topic base part */
	src += config->topic_base_len;

	/* user sends commands to republished topic, so for example
	 * /iot/office/blinds/roller/0/command so we have to find
	 * real shelly id in topic map. */
	el_print(ELD, "%s", src);
	for (node = topic_map; node != NULL; node = node->next)
		if (strncmp(node->dst, src, strlen(node->dst)) == cmp_equal)
			break;

	if (node == NULL)
		return_noval_print(ELW, "unknown command received: %s", msg->topic);

	api_ver = shelly_id_to_ver(node->src);
	if (api_ver == -1)
		return_noval_print(ELW, "unkown api version");

	/* src already points past base topic, move it by length of
	 * user's shelly id to get shelly specific part of topic, */
	src += strlen(node->dst) + 1;

	if (api_ver == 1)
	{
		/* for api version 1, we just have to
		 * simply change topic and republish */
		/* construct new topic */
		snprintf(topic, sizeof(topic), "shellies/%s/%s", node->src, src);
		/* and republish msg */
		ret = mosquitto_publish(mqtt, NULL, topic,
			msg->payloadlen, msg->payload, msg->qos, msg->retain);
		if (ret)
			el_print(ELW, "error republishing %s to %s, reason: %s",
					msg->topic, topic, mosquitto_strerror(ret));
		return;
	}

	/* api verion 2 */
	/* src is pointing past base topic and id, so first part
	 * now is command, like relay or roller */
	cmd = src;
	while (*src != '/') src++;
	/* replace '/' with '\0', so cmd contain valid c-string
	 * that contains command like, rolle, relay or light etc. */
	*src = '\0';
	src++;

	/* get id of subdevice, like shelly2.5 in normal mode will
	 * have 2 relays, so id will be either 0 or 1, for shelly1
	 * id can only be 0 */
	id = src;
	while (*src != '/') src++;
	*src = '\0';
	src++;

	if (strcmp(cmd, "relay") == cmp_equal)
	{
		/* relay in v1 is a switch component in v2
		 * https://shelly-api-docs.shelly.cloud/gen2/Components/FunctionalComponents/Switch */

		/* construct json message, it's simple message so no need to
		 * use jansson lib for it, sprintf will do just fine */
		sprintf(cmd_json,
			"{\"id\":1, \"src\": \"%s\", \"method\":\"Switch.Set\", "
			"\"params\":{\"id\":%s, \"on\":%s}}", node->dst, id,
			strcmp(msg->payload, "on") == cmp_equal ? "true" : "false");
	}

	/* construct new topic */
	snprintf(topic, sizeof(topic), "%s/rpc", node->src);

	el_print(ELD, "v2: cmd publish: %s:%s", topic, cmd_json);
	ret = mosquitto_publish(mqtt, NULL, topic,
		strlen(cmd_json), cmd_json, msg->qos, msg->retain);
	if (ret)
		el_print(ELW, "error republishing %s to %s, reason: %s",
				msg->topic, topic, mosquitto_strerror(ret));

}


/* ==========================================================================
    Called by mqtt_on_message when shelly v1 message format is received
   ========================================================================== */
static void mqtt_on_message_v1
(
	struct mosquitto                *mqtt,     /* mqtt session */
	void                            *userdata, /* not used */
	const struct mosquitto_message  *msg       /* received message */
)
{
	(void)userdata;
	char                            *t;        /* ptr to somewhere in rtopic */
	const char                      *dst;      /* where to republish message */
	char                            *shelly_id;/* shelly id from topic */
	int                              ret;      /* return from mosquitto_pub */
	char                             rtopic[TOPIC_MAX]; /* received topic */
	char                             topic[TOPIC_MAX]; /* topic to publish msg*/
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	rtopic[sizeof(rtopic) - 1] = '\0';
	strncpy(rtopic, msg->topic, sizeof(rtopic));
	if (rtopic[sizeof(rtopic) - 1] != '\0')
		return_noval_print(ELW, "received topic too long: %s", msg->topic);

	/* v1 topics will be in format similar to this
	 *   shellies/shellyplug-s-6F3458/relay/0 */

	/* skip shellies/ part of topic (9 bytes) */
	t = shelly_id = rtopic + 9;
	/* find next / in received topic */
	while (*t != '/') t++;
	/* and replace it with '/0', shelly_id will not contain
	 * valid c-string with shelly-id */
	*t = '\0';
	/* make t to point to device specific topic part,
	 * in our exampel case it will be "relay/0" */
	t++;

	dst = id_map_find(topic_map, shelly_id);
	if (dst == NULL)
	{
		el_print(ELW, "%s not found in map, how?!", dst);
		return;
	}

	snprintf(topic, sizeof(topic), "%s%s/%s", config->topic_base, dst, t);
	el_print(ELD, "republish v1 %s -> %s", msg->topic, topic);
	ret = mosquitto_publish(mqtt, NULL, topic,
		msg->payloadlen, msg->payload, msg->qos, msg->retain);
	if (ret)
		el_print(ELW, "error republishing %s to %s, reason: %s",
				msg->topic, topic, mosquitto_strerror(ret));
}


/* ==========================================================================
    Called by mqtt_on_message when shelly v2 message format is received
   ========================================================================== */
static void mqtt_on_message_v2
(
	struct mosquitto                *mqtt,     /* mqtt session */
	void                            *userdata, /* not used */
	const struct mosquitto_message  *msg       /* received message */
)
{
	const char                      *dst;      /* where to republish message */
	char                            *src;      /* who sent us a message */
	char                             rtopic[TOPIC_MAX]; /* received topic */
	char                             topic[TOPIC_MAX]; /* topic to publish msg*/
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


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
	 * assuming that shelly id is mapped to heat/office
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
	/* move back src pointer to the beginning and skip "shelly" from
	 * device name */
	src = rtopic + 6;


#define publish_for_device(d) \
	if (strcmp(#d, src) == cmp_equal) { \
		shelly_##d##_pub(topic, msg->payload, msg->qos, msg->retain); \
		return; \
	}

	publish_for_device(plus1pm);
#undef publish_for_device

	/* if we get here, that means we received message for
	 * unsupported device */
	el_print(ELW, "unsupported shelly device: %s, please report a bug", src);
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
	const char                      *dst;      /* where to republish message */
	char                            *src;      /* who sent us a message */
	char                             rtopic[TOPIC_MAX]; /* received topic */
	char                             topic[TOPIC_MAX]; /* topic to publish msg*/
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	unused(userdata);

	if (strstr(msg->topic, "/command"))
	{
		if (strncmp(msg->topic, "shellies/", 9) == cmp_equal)
			/* either user directly controlls shelly (without us)
			 * or it's message sent by us, either way ignore message
			 * to avoid some infinite loops and broken network */
			return;

		/* command can be trigger only by the user,
		 * and never by shelly */
		mqtt_on_message_cmd(mqtt, userdata, msg);
		return;
	}


	if (strncmp(msg->topic, "shellies/", 9) == cmp_equal)
	{
		mqtt_on_message_v1(mqtt, userdata, msg);
		/* there is no translation for v1 messages, only
		 * republishing with different topic */
		return;
	}

	/* shelly v2 messages */
	mqtt_on_message_v2(mqtt, userdata, msg);
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


/* ==========================================================================
                ____ ___   ____ _ / /_ / /_   ____   __  __ / /_
               / __ `__ \ / __ `// __// __/  / __ \ / / / // __ \
              / / / / / // /_/ // /_ / /_   / /_/ // /_/ // /_/ /
             /_/ /_/ /_/ \__, / \__/ \__/  / .___/ \__,_//_.___/
                           /_/            /_/
   ==========================================================================
    Publishes bool as "on" or "off"
   ========================================================================== */
void mqtt_pub_bool
(
	const char  *btopic,       /* first (base) part of topic (base+device id) */
	const char  *topic,        /* second part of topic, device specific data */
	int          val,          /* 0 - off, !0 - on */
	int          qos,          /* qos to send message with */
	int          retain        /* mqtt retain flag */
)
{
	char         payload[4];   /* on or off */
	char         t[TOPIC_MAX]; /* final topic to send message to */
	int          ret;          /* ret code from mosquitto_publish */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	t[0] = '\0';
	strcat(t, btopic);
	strcat(t, topic);
	strcpy(payload, val ? "on" : "off");
	el_print(ELD, "mqtt-pub-bool: %s: %s", t, payload);
	ret = mosquitto_publish(g_mqtt, NULL, t,
			strlen(payload)+1, payload, qos, retain);
	if (ret)
		el_print(ELW, "error sending %s to %s, reason: %s", payload, t,
				mosquitto_strerror(ret));
}


/* ==========================================================================
    Publishes string on topic, as is
   ========================================================================== */
void mqtt_pub_string
(
	const char  *btopic,       /* first (base) part of topic (base+device id) */
	const char  *topic,        /* second part of topic, device specific data */
	const char  *payload,      /* payload to send */
	int          qos,          /* qos to send message with */
	int          retain        /* mqtt retain flag */
)
{
	char         t[TOPIC_MAX]; /* final topic to send message to */
	int          ret;          /* ret code from mosquitto_publish */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	t[0] = '\0';
	strcat(t, btopic);
	strcat(t, topic);
	el_print(ELD, "mqtt-pub: %s: %s", t, payload);
	ret = mosquitto_publish(g_mqtt, NULL, t,
			strlen(payload)+1, payload, qos, retain);
	if (ret)
		el_print(ELW, "error sending %s to %s, reason: %s", payload, t,
				mosquitto_strerror(ret));
}


/* ==========================================================================
    Converts double $num to string, and the publishes message
   ========================================================================== */
void mqtt_pub_number
(
	const char  *btopic,     /* first (base) part of topic (base+device id) */
	const char  *topic,      /* second part of topic, device specific data */
	double       num,        /* number to publish (as string) */
	int          qos,        /* qos to send message with */
	int          retain,     /* mqtt retain flag */
	int          precision   /* float number precision */
)
{
	char         payload[128]; /* data to send over mqtt */
	char         t[TOPIC_MAX]; /* final topic to send message to */
	int          ret;          /* ret code from mosquitto_publish */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	t[0] = '\0';
	strcat(t, btopic);
	strcat(t, topic);
	snprintf(payload, sizeof(payload), "%.*f", precision, num);
	el_print(ELD, "mqtt-pub: %s: %s", t, payload);
	ret = mosquitto_publish(g_mqtt, NULL, t,
			strlen(payload)+1, payload, qos, retain);
	if (ret)
		el_print(ELW, "error sending %s to %s, reason: %s", payload, t,
				mosquitto_strerror(ret));

}



