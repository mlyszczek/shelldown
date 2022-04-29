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


#include "config.h"

#if HAVE_CONFIG_H
#	include "shelldown-config.h"
#endif

#include <embedlog.h>
#include <errno.h>
#include <getopt.h>
#include <ini.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */

/* declarations to make life easier when parsing same thing over
 * and over again */
#define PARSE_INT(OPTNAME, OPTARG, MINV, MAXV) \
	{ \
		long   val;	 /* value converted from name */ \
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/ \
		\
		if (config_get_number(OPTARG, &val) != 0) \
		{ \
			fprintf(stderr, "%s, invalid number %s", #OPTNAME, OPTARG); \
			return -1; \
		} \
		\
		if (val < MINV || MAXV < val) \
		{ \
			fprintf(stderr, "%s: bad value %s(%ld), min: %ld, max: %ld\n", \
					#OPTNAME, OPTARG, (long)val, (long)MINV, (long)MAXV); \
			return -1; \
		} \
		g_config.OPTNAME = val; \
	}

#define VALID_STR(OPTNAME, OPTARG) \
	size_t optlen; \
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/ \
	\
	optlen = strlen(OPTARG); \
	\
	if (optlen >= sizeof(g_config.OPTNAME)) \
	{ \
		fprintf(stderr, "%s: is too long %s(%ld),  max: %ld\n", \
				#OPTNAME, optarg, (long)optlen, \
				(long)sizeof(g_config.OPTNAME)); \
		return -1; \
	}

#define PARSE_MAP(OPTNAME, MAPNAME) \
	{ \
		long long  val; \
		if (config_parse_map(MAPNAME, \
					&val, g_shelldown_ ## OPTNAME ## _map) != 0) \
			return -1; \
		g_config.OPTNAME = val; \
	}

#define PARSE_STR(OPTNAME, OPTARG) \
	{ \
		VALID_STR(OPTNAME, OPTARG); \
		strcpy(g_config.OPTNAME, OPTARG); \
	}

#define PARSE_INT_INI_NS(OPTNAME, MINV, MAXV) \
	PARSE_INT(OPTNAME, value, MINV, MAXV)

#define PARSE_INT_INI(SECTION, OPTNAME, MINV, MAXV) \
	PARSE_INT(SECTION ## _ ## OPTNAME, value, MINV, MAXV)

#define PARSE_STR_INI_NS(OPTNAME) \
	PARSE_STR(OPTNAME, value)

#define PARSE_STR_INI(SECTION, OPTNAME) \
	PARSE_STR(SECTION ## _ ## OPTNAME, value)

#define PARSE_MAP_INI_NS(OPTNAME) \
	PARSE_MAP(OPTNAME, value)

#define PARSE_MAP_INI(SECTION, OPTNAME) \
	PARSE_MAP(SECTION ## _ ## OPTNAME, value)

/* list of short options for getopt_long */
static const char *shortopts = ":hvc:dDh:p:";


/* array of long options for getop_long. This is defined as macro so it
 * can be used in functions scope. If we would have delacred it as static
 * here, then it would take that memory for the whole duration of app,
 * and there is no need for it - instead it will take memory only in
 * functions' stack */
#define STRUCT_OPTION_LONGOPTS \
	const struct option longopts[] = \
	{ \
		{"help",      no_argument,       NULL, 'h'}, \
		{"version",   no_argument,       NULL, 'v'}, \
		{"config",    required_argument, NULL, 'c'}, \
		{"debug",     no_argument,       NULL, 'd'}, \
		{"daemon",    no_argument,       NULL, 'D'}, \
		{"logfile",   required_argument, NULL, 'l'}, \
		{"mqtt-host", required_argument, NULL, 'm'}, \
		{"mqtt-port", required_argument, NULL, 'p'}, \
 \
		{NULL, 0, NULL, 0} \
	}

struct config_map
{
	const char  *str;
	long long    val;
};

/* define config object in static storage duration - c'mon, you
 * won't be passing pointer to config to every function, will you? */
static struct config  g_config;

/* define constant pointer to config object - it will be
 * initialized in init() function. It's const because you really
 * shouldn't modify config once it's set, if you need mutable
 * config, you will be better of creating dedicated module for that
 * and store data in /var/lib instead. This config module should
 * only be used with configs from /etc which should be readonly by
 * the programs */
const struct config  *config;

/* arrays of strings of options that have mapped values, used
 * to map config ints back into strings for nice config_dump() */



/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Parses map of strings and if arg is found in map, it's numerical value
    (map index) is stored in field
   ========================================================================== */
static int config_parse_map
(
	const char  *aargs,   /* string(s) to search in a map */
	long long   *field,   /* numerical value for arg will be stored here */
	const struct config_map  *map
)
{
	const struct config_map  *maps;
	char         args[0];
	char        *s;
	char        *sp;
	int          ret;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	maps = map;
	strcpy(args, aargs);
	*field = 0;

	for (s = strtok_r(args, ",", &sp); s != NULL; s = strtok_r(NULL, ",", &sp))
	{
		for (ret = -1, map = maps; map->str != NULL; ++map)
		{
			if (strcmp(map->str, s) == 0)
			{
				*field += map->val;
				ret = 0;
			}
		}

		if (ret == -1)
		{
			fprintf(stderr, "error, invalid option: %s\n", s);
			return -1;
		}
	}

	return ret;
}


/* ==========================================================================
    Parses map of strings and if numerical arg is found in map, pointer to
    string for that value is returned, or NULL if not found.
   ========================================================================== */
static const char * config_parse_map_by_value
(
	long long    val,     /* value to search in a map */
	int          flag,    /* is val flag type or single value? */
	char        *ret,     /* buf to store flag strings, used when flag == 1 */
	const struct config_map  *map
)
{
	if (flag == 0)
		for (; map->str != NULL; ++map)
			if (map->val == val)
				return map->str;

	ret[0] = '\0';
	for (; map->str != NULL; ++map)
		if (val & map->val)
		{
			strcat(ret, map->str);
			strcat(ret, ",");
		}

	/* remove trailing comma */
	ret[strlen(ret) - 1] = '\0';
	return ret;
}



/* ==========================================================================
    Guess what! It prints help into stdout! Would you belive that?
   ========================================================================== */
static int config_print_help
(
	const char  *name  /* program name (argv[0]) */
)
{
	printf(
"Usage: %s [-h | -v | options]\n"
"\n"
"options:\n"
"\t-h, --help                print this help and exit\n"
"\t-v, --version             print version information and exit\n"
"\t-c, --config=<file>       config file to use\n"
"\t-l, --logfile=<path>      where to store logs\n"
"\t-d, --debug               enable debug logging\n"
"\t-D, --daemon              run as daemon\n"
"\t-m, --mqtt-host=<ip>      broker ip address\n"
"\t-p, --mqtt-port=<port>    broker port\n"

, name);

	return 0;
}


/* ==========================================================================
    Print version and author of the program. And what else did you thing
    this function might do?
   ========================================================================== */
static void config_print_version
(
	void
)
{
	printf("shelldown "PACKAGE_VERSION"\n"
			"by Michał Łyszczek <michal.lyszczek@bofc.pl>\n");
}


/* ==========================================================================
    Converts string number 'num' into number representation. Converted value
    will be stored in address pointed by 'n'.
   ========================================================================== */
static int config_get_number
(
	const char  *num,  /* string to convert to number */
	long        *n     /* converted num will be placed here */
)
{
	const char  *ep;   /* endptr for strtol function */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	if (*num == '\0')
	{
		errno = EINVAL;
		return -1;
	}

	*n = strtol(num, (char **)&ep, 10);

	if (*ep != '\0')
	{
		errno = EINVAL;
		return -1;
	}

	if (*n == LONG_MAX || *n == LONG_MIN)
	{
		errno = ERANGE;
		return -1;
	}

	return 0;
}


/* ==========================================================================
    Parse arguments passed from ini file. This funciton is called by inih
    each time it reads valid option from file.
   ========================================================================== */
static int config_parse_ini
(
	void        *user,     /* user pointer - not used */
	const char  *section,  /* section name of current option */
	const char  *name,     /* name of current option */
	const char  *value,    /* value of current option */
	int          lineno    /* line of current option */
)
{
	(void)user;
	(void)lineno;

	/* parsing section  */

	if (strcmp(section, "") == 0)
    {
		if (strcmp(name, "debug") == 0)
			PARSE_INT_INI_NS(debug, 0, 1)
		else if (strcmp(name, "daemon") == 0)
			PARSE_INT_INI_NS(daemon, 0, 1)
	}

	/* parsing section mqtt */

	else if (strcmp(section, "mqtt") == 0)
    {
		if (strcmp(name, "host") == 0)
			PARSE_STR_INI(mqtt, host)
		else if (strcmp(name, "port") == 0)
			PARSE_INT_INI(mqtt, port, 1, 65535)
	}

	/* as far as inih is concerned, 1 is OK, while 0 would be error */
	return 1;
}


/* ==========================================================================
    Parse arguments passed from command line using getopt_long
   ========================================================================== */
static int config_parse_args
(
	int    argc,
	char  *argv[]
)
{
	int    arg;      /* current option being parsed */
	int    loptind;  /* current long option index */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	STRUCT_OPTION_LONGOPTS;
	optind = 1;
	while ((arg = getopt_long(argc, argv, shortopts, longopts, &loptind)) != -1)
	{
		switch (arg)
		{
		case 'm': config_print_help(argv[0]); return -2;
		case 'v': config_print_version(); return -3;
		case 'c': /* already parsed, ignore */; break;
		case 'd': g_config.debug = 1; break;
		case 'D': g_config.daemon = 1; break;
		case 'l': PARSE_STR(logfile, optarg); break;
		case 'h': PARSE_STR(mqtt_host, optarg); break;
		case 'p': PARSE_INT(mqtt_port, optarg, 1, 65535); break;


		case ':':
			fprintf(stderr, "option -%c, --%s requires an argument\n",
				optopt, longopts[loptind].name);
			return -1;

		case '?':
			fprintf(stderr, "unknown option -%c (0x%02x)\n", optopt, optopt);
			return -1;

		default:
			fprintf(stderr, "unexpected return from getopt 0x%02x\n", arg);
			return -1;
		}
	}

	return 0;
}


/* ==========================================================================
                       __     __ _          ____
        ____   __  __ / /_   / /(_)_____   / __/__  __ ____   _____ _____
       / __ \ / / / // __ \ / // // ___/  / /_ / / / // __ \ / ___// ___/
      / /_/ // /_/ // /_/ // // // /__   / __// /_/ // / / // /__ (__  )
     / .___/ \__,_//_.___//_//_/ \___/  /_/   \__,_//_/ /_/ \___//____/
    /_/
   ========================================================================== */



/* ==========================================================================
    Parses options in following order (privided that parsing was enabled
    during compilation).

    - set option values to their well-known default values
    - if proper c define (-D) is enabled, overwrite that option with it
    - overwrite options specified in ini
    - overwrite options passed by command line
   ========================================================================== */
int config_init
(
	int    argc,   /* number of arguments in argv */
	char  *argv[]  /* program arguments from command line */
)
{
	const char  *file;   /* path to ini config file */
	int          arg;    /* current argument from getopt() */
	STRUCT_OPTION_LONGOPTS;
	int          ret;    /* function return value */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	(void)argc;
	(void)argv;
	ret = 0;
	memset(&g_config, 0x00, sizeof(g_config));

	/* disable error printing from getopt library, some getopt() libraries
	 * (like the one in nuttx) don't support opterr, so we add define to
	 * disable this set. By default it is enabled though. To disable it
	 * just pass -DSHELLDOWN_NO_OPTERR to compiler. */
#	ifndef SHELLDOWN_NO_OPTERR
	opterr = 0;
#	endif

	/* set config with default, well known values */
	g_config.debug = 0;
	g_config.daemon = 0;
	strcpy(g_config.mqtt_host, "127.0.0.1");
	g_config.mqtt_port = 1883;

	/* overwrite values with those define in compiletime */
#ifdef SHELLDOWN_CONFIG_DEBUG
	g_config.debug = SHELLDOWN_CONFIG_DEBUG;
#endif

#ifdef SHELLDOWN_CONFIG_DAEMON
	g_config.daemon = SHELLDOWN_CONFIG_DAEMON;
#endif

#ifdef SHELLDOWN_CONFIG_MQTT_HOST
	g_config.mqtt_host = SHELLDOWN_CONFIG_MQTT_HOST;
#endif

#ifdef SHELLDOWN_CONFIG_MQTT_PORT
	g_config.mqtt_port = SHELLDOWN_CONFIG_MQTT_PORT;
#endif


	/* next, process .ini file */

	/* custom file can only be passed via command line if command
	 * line parsing is enabled, right? */
	arg = -1;
	optind = 1;
	file = SHELLDOWN_CONFIG_PATH_DEFAULT;

	/* we need to scan arg list to check if user provided custom
	 * file path. Even though we only look for option "-c" we need
	 * to use full shortopts with all expected arguments or bug may
	 * occur. getopt(3) can take multiple arguments (flags) with
	 * single '-', like -arg, where a, r and g are different flags
	 * that also could be specified like -a -r -g.  This is for
	 * convinience, but if not carefull can lead to tricky bug.
	 * Consider user argument "-lnotice" where -l is option to
	 * determing log level and notice is its argument.  Now
	 * shortopts is only "c:" so getopt(3) does not know that
	 * -l takes an argument and it automatically assumes it does
	 * not. So getopt(3) will check for -l and that will return
	 * unknown option error, then it will check 'n' (yes!), o, t,
	 * i and then "c". At this point getopt(3) knows that "c"
	 * takes argument which in this case will be "e". To avoid
	 * that, we getopt(3) need to know that "-l" takes an
	 * argument, and for that we need to pass every option we
	 * support to shortopts. */
	while ((arg = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
	{
		if (arg == 'c')
		{
			file = optarg;

			/* since we are looking only for "-c" argument, when we
			 * find it, we can stop searching */
			break;
		}

		/* ignore any unknown options, time will come to parse them too */
	}

	/* parse options from .ini config file, overwritting default
	 * and/or compile time options */
	if (ini_parse(file, config_parse_ini, NULL) != 0)
	{
		/* failed to parse config file.
		 *
		 * But if ini file does not exist, do not crash the app,
		 * and let it run with default options. Only crash app
		 * when user explicitly passed config with '-c'. It is
		 * obvious then that user really wants custom option, so
		 * if they are not available, he should be notified. */
		if (arg != 'c' && errno == ENOENT)
			goto not_an_error_after_all;

		fprintf(stderr, "error while parsing config file: %s: %s\n",
				file, strerror(errno));
		return -1;
	}

not_an_error_after_all:
	/* parse options passed from command line - these have the
	 * highest priority and will overwrite any other options */
	optind = 1;
	ret = config_parse_args(argc, argv);

	/* all good, initialize global config pointer with config
	 * object */
	config = (const struct config *)&g_config;
	return ret;
}



/* ==========================================================================
    Dumps content of config to place which has been configured in embedlog.

    NOTE: remember to initialize embedlog before calling it!
   ========================================================================== */
void config_dump
(
	void
)
{
	/* macros to make printing easier */

#define CONFIG_PRINT_FIELD(FIELD, MODIFIER) \
	el_print(ELN, "%s%s: "MODIFIER, #FIELD, padder + strlen(#FIELD), \
		g_config.FIELD)

#define CONFIG_PRINT_VAR(VAR, MODIFIER) \
	el_print(ELN, "%s%s: "MODIFIER, #VAR, padder + strlen(#VAR), VAR)

#define CONFIG_PRINT_MAP(FIELD, FLAG) \
	{ \
		char  buf[0]; \
		el_print(ELN, "%s%s: %s", #FIELD, padder + strlen(#FIELD), \
			config_parse_map_by_value(g_config.FIELD, \
				FLAG, buf, g_shelldown_ ## FIELD ## _map)); \
	}

	const char *padder = "........................";
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	el_print(ELN, PACKAGE_STRING);
	el_print(ELN, "shelldown configuration");

	CONFIG_PRINT_FIELD(debug, "%i");
	CONFIG_PRINT_FIELD(daemon, "%i");
	CONFIG_PRINT_FIELD(mqtt_host, "%s");
	CONFIG_PRINT_FIELD(mqtt_port, "%i");


#undef CONFIG_PRINT_FIELD
#undef CONFIG_PRINT_VAR
}
