/* ==========================================================================
    Licensed under bsd2 license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include "config.h"

#include <embedlog.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "id-map.h"

/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


static volatile int g_shelldown_run;


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Trivial signal handler for SIGINT and SIGTERM, to shutdown program with
    grace.
   ========================================================================== */


static void sigint_handler
(
	int signo  /* signal that triggered this handler */
)
{
	(void)signo;

	g_shelldown_run = 0;
}


/* ==========================================================================
                                              _
                           ____ ___   ____ _ (_)____
                          / __ `__ \ / __ `// // __ \
                         / / / / / // /_/ // // / / /
                        /_/ /_/ /_/ \__,_//_//_/ /_/

   ========================================================================== */


#if SHELLDOWN_ENABLE_LIBRARY
int shelldown_main
#else
int main
#endif
(
	int    argc,   /* number of arguments in argv */
	char  *argv[]  /* array of passed arguments */
)
{
	int    ret;    /* return code from the program */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	{
		/* install SIGTERM and SIGINT signals for clean exit. */
		struct sigaction  sa;  /* signal action instructions */
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		g_shelldown_run = 1;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = sigint_handler;
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
	}

	/* first things first, initialize configuration of the program */
	ret = config_init(argc, argv);

	if (ret == -1)
		/* critical error occured when config was being parsed,
		 * in such case we don't want to continue. */
		return 1;

	if (ret == -2)
		/* it was decided by config that program should not run,
		 * but we should not exit with and error. This happens
		 * when either -v or -h was passed */
		return 0;

	/* I am pessimist in nature, so I assume things will screw
	 * up from now on */
	ret = 1;


	/* Initialize logger. Should it fail? Fine, we care only a
	 * little bit but not that much to crash application so even if
	 * it fails we still continue with our program. Of course if
	 * logger fails program will probably fail too, but meh... one
	 * can allow himself to be optimistic from time to time.
	 */

	if (el_init() == 0)
	{
		/* logger init succeed, configure it */

		el_option(EL_FROTATE_NUMBER, 0);
		el_option(EL_FROTATE_SIZE, -1);
		el_option(EL_FSYNC_EVERY, 65536);
		el_option(EL_FSYNC_LEVEL, EL_WARN);
		el_option(EL_TS, EL_TS_LONG);
		el_option(EL_TS_TM, EL_TS_TM_REALTIME);
		el_option(EL_TS_FRACT, EL_TS_FRACT_OFF);
		el_option(EL_FINFO, 1);
		el_option(EL_COLORS, 1);
		el_option(EL_PREFIX, "shelldown: ");
		el_option(EL_LEVEL, EL_INFO);

		if (config->debug)
		{
			el_option(EL_OUT, EL_OUT_STDERR);
			el_option(EL_LEVEL, EL_DBG);
		}
		else
		{
			/* we will be outputing logs to file, so
			 * we need to open file now */
			el_option(EL_OUT, EL_OUT_FILE);
			if (el_option(EL_FPATH, config->log_file) != 0)
			{
				if (errno == ENAMETOOLONG || errno == EINVAL)
					/* in general embedlog will try to recover from
					 * any error that it may stumble upon (like
					 * directory does not yet exist - but will be
					 * created later, or permission is broker, but
					 * will be fixed later). That errors could be
					 * recovered from with some external help so
					 * there is no point disabling file logging.
					 * Any errors, except for these two.  In this
					 * case, disable logging to file as it is
					 * pointless, so we disable logging to file
					 * leaving other destinations intact. As last
					 * resort, we will try to print to stderr. */
					el_option(EL_OUT, EL_OUT_STDERR);

				/* print warning to stderr so it's not missed by
				 * integrator in case file output was the only
				 * output enabled */
				fprintf(stderr, "w/failed to open log file %s, %s\n",
						config->log_file, strerror(errno));
			}
		}
	}

	/* dump config, it's good to know what is program configuration
	 * when debugging later */
	config_dump();

	/* ================================= */
	/* put your initialization code here */
	/* ================================= */

	/* all resources initialized, now start main loop */

	el_print(ELN, "all resources initialized, starting main loop");


	while (g_shelldown_run)
	{
		/* ======================= */
		/* put your main code here */
		/* ======================= */
	}

	/* ========================== */
	/* put your cleanup code here */
	/* ========================== */

	ret = 0;


	el_print(ELN, "goodbye %s world!", ret ? "cruel" : "beautiful");
	el_cleanup();

	return ret;
}
