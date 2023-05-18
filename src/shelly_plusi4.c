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


/* ==========================================================================
    shelly i4 params format is. 2 in this case means, button with id 2
    was pressed. This 2 is two times, in "input:2" and then in "id".

      {
        "params": {
          "ts": 1684420693.03,
          "input:2": {
            "id": 2,
            "state": true
          }
        }
      }
   ========================================================================== */
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
	json_t      *input;      /* input component */
	json_t      *state;      /* pressed button state */
	int          btn_id;     /* which button was pressed on shelly 0-3 */
	char         t[32];      /* button specific topic to publish on */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	el_print(ELD, "shelly plusi4 pub");
	root = json_loads(payload, 0, NULL);
	if (root == NULL)
		goto_print(error, ELW, "[si4] invalid json received %s", payload);

	json_object_get_or_error("si4", params, root, "params");
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
	json_decref(root);
}


