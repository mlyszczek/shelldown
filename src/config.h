/* ==========================================================================
    !!! File generated with progen project. Any changes will overwritten !!!
   ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef SHELLDOWN_CONFIG_H
#define SHELLDOWN_CONFIG_H 1

#if HAVE_CONFIG_H
#	include "shelldown-config.h"
#endif

#include <limits.h>
#include <stddef.h>

struct config
{
	/* enable debug logging */
	int  debug;

	/* run as daemon */
	int  daemon;

	/* where logs should be stored */
	char log_file[PATH_MAX];

	/* path to a file with from-to map */
	char id_map_file[PATH_MAX];

	/* base topic added to all messages */
	char topic_base[128];
	size_t topic_base_len;

	/* broker ip address */
	char  mqtt_host[15 + 1];

	/* broker port */
	int  mqtt_port;

};

extern const struct config  *config;
int config_init(int argc, char *argv[]);
void config_dump();

#endif
