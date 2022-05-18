/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/
   ========================================================================== */

#include "shelly.h"

#include <embedlog.h>
#include <string.h>

#include "macros.h"

/* ==========================================================================
              / __/__  __ ____   _____ / /_ (_)____   ____   _____
             / /_ / / / // __ \ / ___// __// // __ \ / __ \ / ___/
            / __// /_/ // / / // /__ / /_ / // /_/ // / / /(__  )
           /_/   \__,_//_/ /_/ \___/ \__//_/ \____//_/ /_//____/
   ========================================================================== */


/* ==========================================================================
    Returns API version number for given shelly id.
   ========================================================================== */
int shelly_id_to_ver
(
	const char  *id  /* shelly id (like shellyplus1pm-7c87ce65bd9c) */
)
{
#define RET_API(s, v) if (strncmp(id, s, strlen(s)) == cmp_equal) return v

	RET_API("shellyplug", 1);
	RET_API("shellyswitch25", 1);
	RET_API("shellyem3", 1);

	RET_API("shellyplus1pm", 2);

#undef RET_API
	el_print(ELW, "unkown shelly id: %s, please, report this bug", id);
	return -1;
}
