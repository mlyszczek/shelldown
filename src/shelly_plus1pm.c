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


/* ==========================================================================
              / __/__  __ ____   _____ / /_ (_)____   ____   _____
             / /_ / / / // __ \ / ___// __// // __ \ / __ \ / ___/
            / __// /_/ // / / // /__ / /_ / // /_/ // / / /(__  )
           /_/   \__,_//_/ /_/ \___/ \__//_/ \____//_/ /_//____/
   ========================================================================== */


void shelly_plus1pm_pub
(
	const char  *topic,      /* part of topic (base + device id) */
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
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	el_print(ELD, "shelly plus1pm pub");
	root = json_loads(payload, 0, NULL);
	if (root == NULL)
		goto_print(error, ELW, "[s1pm] invalid json received %s", payload);

	/* shelly plus pm1 has only one switch, so id will always be 0 */
	json_object_get_or_error("s1pm", params, root, "params");
	json_object_get_or_error("s1pm", swtch, params, "switch:0");
	json_object_foreach(swtch, key, value)
	{
		if (strcmp(key, "apower") == cmp_equal)
			mqtt_pub_number(topic, "relay/0/power", json_number_value(value),
					qos, retain, 2);

		else if (strcmp(key, "voltage") == cmp_equal)
			mqtt_pub_number(topic, "relay/0/voltage", json_number_value(value),
					qos, retain, 2);

		else if (strcmp(key, "output") == cmp_equal)
			mqtt_pub_bool(topic, "relay/0", json_boolean_value(value),
					qos, retain);

		else if (strcmp(key, "temperature") == cmp_equal)
		{
			json_t  *temp_c;  /* internal temp in °C */
			json_t  *temp_f;  /* internal temp in °F */
			double   temp;    /* temp for temp status calculation */
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


			json_object_get_or_error("s1pm", temp_c, value, "tC");
			json_object_get_or_error("s1pm", temp_f, value, "tF");

			mqtt_pub_number(topic, "temperature", json_number_value(temp_c),
					qos, retain, 1);
			mqtt_pub_number(topic, "temperature_f", json_number_value(temp_c),
					qos, retain, 1);

			temp = json_number_value(temp_c);
			if (temp > VHIGH_TEMP)
				mqtt_pub_string(topic, "temperature_status", "Very High", 2, 1);
			else if (temp > HIGH_TEMP)
				mqtt_pub_string(topic, "temperature_status", "High", 2, 1);
			else
				mqtt_pub_string(topic, "temperature_status", "Normal", 2, 1);
		}

		else if ((strcmp(key, "id") & strcmp(key, "source") &
				strcmp(key, "timer_started_at") &
				strcmp(key, "timer_duration") &
				strcmp(key, "aenergy")) == cmp_equal)
			continue; /* ignore unusable fields */

		else
			el_print(ELN, "unkown key received: %s, please report "
					"bug for missing key, so it can be ignored or "
					"implemented", key);
	}


error:
	json_decref(root);
}


