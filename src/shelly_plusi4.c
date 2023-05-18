/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/
   ========================================================================== */

#ifdef HAVE_CONFIG_H
#   include "shelldown-config.h"
#endif

#include "shelly.h"

#include <jansson.h>
#include <embedlog.h>
#include <errno.h>
#include <string.h>

#include "macros.h"
#include "mqtt.h"
#include "id-map.h"

static id_map_t g_button_state;


/* ==========================================================================
              / __/__  __ ____   _____ / /_ (_)____   ____   _____
             / /_ / / / // __ \ / ___// __// // __ \ / __ \ / ___/
            / __// /_/ // / / // /__ / /_ / // /_/ // / / /(__  )
           /_/   \__,_//_/ /_/ \___/ \__//_/ \____//_/ /_//____/
   ========================================================================== */


/* ==========================================================================
    Format with switch mode
      {
        "params": {
          "ts": 1684420693.03,
          "input:2": {
            "id": 2,
            "state": true
          }
        }
      }

    Format with button mode
      {
        "params": {
          "ts": 1684424637.72,
          "events": [
            {
              "component": "input:1",
              "id": 1,
              "event": "btn_up", # or btn_down
              "ts": 1684424637.72
            }
          ]
        }
      }
   ========================================================================== */
void shelly_plusi4_handle_events
(
	const char  *topic,      /* part of topic (base + device id) */
	const char  *payload,    /* jsonrpc payload from shelly */
	int          qos,        /* qos to send message with */
	int          retain,     /* mqtt retain flag */
	json_t      *root        /* root part of json payload */
)
{
	const char  *src;        /* source shelly (shelly id) */
	json_t      *json_src;   /* src part of json payload */
	json_t      *value;      /* json value for foreach loop */
	json_t      *events;     /* events part of json payload */
	json_t      *params;     /* params part of json payload */
	json_t      *obj;        /* generic json object */
	int          btn_state;  /* state of button as int */
	const char  *btn_event;  /* button event */
	int          btn_id;     /* id of button pressed */
	size_t       index;      /* json array index */
	char         t[32];      /* button specific topic to publish on */
	id_map_t     btn_sstate; /* button saved state between calls */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	json_object_get_or_error("si4", json_src, root, "src");
	json_object_get_or_error("si4", params, root, "params");
	json_object_get_or_error("si4", events, params, "events");

	src = json_string_value(json_src);
	btn_sstate = id_map_find_node(g_button_state, src, NULL);
	if (btn_sstate == NULL)
		id_map_add_state(&g_button_state, src, 0);
	btn_sstate = id_map_find_node(g_button_state, src, NULL);

	json_array_foreach(events, index, value)
	{
		json_object_get_or_error("si4", obj, value, "event");
		btn_event = json_string_value(obj);
		if (btn_event == NULL)
			continue;

		if (strcmp(btn_event, "btn_down"))
			/* we only care for button down events */
			continue;

		json_object_get_or_error("si4", obj, value, "id");
		btn_id = json_integer_value(obj);

		/* toggle current button state and publish new state */
		btn_sstate->state ^= 1 << btn_id;
		btn_state = !!(btn_sstate->state & 1 << btn_id);

		sprintf(t, "input/%d", btn_id);
		mqtt_pub_bool(topic, t, btn_state, qos, retain);
	}

error:
	/* just return, json will be decrefed in upper function */
	return;
}

void shelly_plusi4_handle_input
(
	const char  *topic,      /* part of topic (base + device id) */
	const char  *payload,    /* jsonrpc payload from shelly */
	int          qos,        /* qos to send message with */
	int          retain,     /* mqtt retain flag */
	json_t      *params      /* param section of json payload */
)
{
	json_t      *input;      /* input component (for switch mode) */
	json_t      *state;      /* pressed button state */
	int          btn_id;     /* which button was pressed on shelly 0-3 */
	char         t[32];      /* button specific topic to publish on */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	for (btn_id = 0; btn_id != 4; btn_id++)
	{
		char input_str[32];
		sprintf(input_str, "input:%d", btn_id);

		if ((input = json_object_get(params, input_str)))
			break;
	}

	if (btn_id == 4)
		goto_print(error, ELW, "[si4] input id not found in msg: %s", payload);

	json_object_get_or_error("si4", state, input, "state");
	sprintf(t, "input/%d", btn_id);
	mqtt_pub_bool(topic, t, json_boolean_value(state), qos, retain);

error:
	/* just return, json will be decrefed in upper function */
	return;
}


void shelly_plusi4_pub
(
	const char  *topic,      /* part of topic (base + device id) */
	const char  *payload,    /* jsonrpc payload from shelly */
	int          qos,        /* qos to send message with */
	int          retain      /* mqtt retain flag */
)
{
	json_t      *root;       /* json message from shelly */
	json_t      *params;     /* params part of json object */
	json_t      *events;     /* events (for button mode) */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	el_print(ELD, "shelly plusi4 pub");
	root = json_loads(payload, 0, NULL);
	if (root == NULL)
		goto_print(error, ELW, "[si4] invalid json received %s", payload);

	json_object_get_or_error("si4", params, root, "params");
	events = json_object_get(params, "events");
	if (events)
		shelly_plusi4_handle_events(topic, payload, qos, retain, root);
	else
		shelly_plusi4_handle_input(topic, payload, qos, retain, params);

error:
	json_decref(root);
}
