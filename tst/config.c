/* ==========================================================================
   !! File generated with progen project. Any changes will get overwritten !!
   ==========================================================================
    Licensed under BSD2clause license See LICENSE file for more information
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

#include "config.h"
#include "mtest.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


mt_defs_ext();
struct shelldown_config  config;


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Prepares for test, called each time test is about to run (if
    mt_prepare_test has been set).
   ========================================================================== */


static void test_prepare(void)
{
    memset(&config, 0, sizeof(config));

    config.debug = 0;
    config.daemon = 0;
    strcpy(config.mqtt_host, "127.0.0.1");
    config.mqtt_port = 1883;

}


/* ==========================================================================
                           __               __
                          / /_ ___   _____ / /_ _____
                         / __// _ \ / ___// __// ___/
                        / /_ /  __/(__  )/ /_ (__  )
                        \__/ \___//____/ \__//____/

   ========================================================================== */


/* ==========================================================================
   ========================================================================== */
static void config_all_default(void)
{
    char  *argv[] = { "shelldown" };
    shelldown_config_init(1, argv);
    mt_fok(memcmp(&config, shelldown_config, sizeof(config)));
}


/* ==========================================================================
   ========================================================================== */
#if SHELLDOWN_ENABLE_GETOPT || SHELLDOWN_ENABLE_GETOPT_LONG

static void config_short_opts(void)
{
    char *argv[] =
    {
        "shelldown",
        "-d",
        "-D",
        "-hghfrdbgjkcx",
        "-p612"

    };
    int argc = sizeof(argv) / sizeof(const char *);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    shelldown_config_init(argc, argv);

    /* seed: 2022-04-30 00:31:59.217537 */
    config.debug = 1;
    config.daemon = 1;
    strcpy(config.mqtt_host, "ghfrdbgjkcx");
    config.mqtt_port = 612;


    mt_fail(example_config->debug == 1);
    mt_fail(example_config->daemon == 1);
    mt_fail(strcmp(example_config->mqtt_host, "ghfrdbgjkcx") == 0);
    mt_fail(example_config->mqtt_port == 612);


    mt_fok(memcmp(&config, shelldown_config, sizeof(config)));
}

#endif


/* ==========================================================================
   ========================================================================== */
#if SHELLDOWN_ENABLE_GETOPT_LONG

static void config_long_opts(void)
{
    char *argv[] =
    {
        "shelldown",
        "--debug",
        "--daemon",
        "--mqtt-host=sonoxdlqc",
        "--mqtt-port=43805"

    };
    int argc = sizeof(argv) / sizeof(const char *);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    shelldown_config_init(argc, argv);

    /* seed: 2022-04-30 00:31:59.209859 */
    config.debug = 1;
    config.daemon = 1;
    strcpy(config.mqtt_host, "sonoxdlqc");
    config.mqtt_port = 43805;


    mt_fail(example_config->debug == 1);
    mt_fail(example_config->daemon == 1);
    mt_fail(strcmp(example_config->mqtt_host, "sonoxdlqc") == 0);
    mt_fail(example_config->mqtt_port == 43805);


    mt_fok(memcmp(&config, shelldown_config, sizeof(config)));
}

#endif


/* ==========================================================================
   ========================================================================== */
#if SHELLDOWN_ENABLE_INI

static void config_from_file(void)
{
    char *argv[] =
    {
        "shelldown",
        "-ctest.ini"
    };
    int argc = sizeof(argv) / sizeof(const char *);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    shelldown_config_init(argc, argv);

    /* seed: 2022-04-30 00:31:59.217965 */
    config.debug = 1;
    config.daemon = 1;
    strcpy(config.mqtt_host, "etfjgzqnsciv");
    config.mqtt_port = 7149;


    mt_fail(example_config->debug == 1);
    mt_fail(example_config->daemon == 1);
    mt_fail(strcmp(example_config->mqtt_host, "etfjgzqnsciv") == 0);
    mt_fail(example_config->mqtt_port == 7149);


    mt_fok(memcmp(&config, shelldown_config, sizeof(config)));

}

#endif
/* ==========================================================================
   ========================================================================== */
static void config_print_help(void)
{
    int    argc = 2;
    char  *argv[] = { "shelldown", "-h", NULL };
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    mt_fail(shelldown_config_init(argc, argv) == -2);
}


/* ==========================================================================
   ========================================================================== */
static void config_print_version(void)
{
    int    argc = 2;
    char  *argv[] = { "shelldown", "-v", NULL };
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    mt_fail(shelldown_config_init(argc, argv) == -3);
}


/* ==========================================================================
             __               __
            / /_ ___   _____ / /_   ____ _ _____ ____   __  __ ____
           / __// _ \ / ___// __/  / __ `// ___// __ \ / / / // __ \
          / /_ /  __/(__  )/ /_   / /_/ // /   / /_/ // /_/ // /_/ /
          \__/ \___//____/ \__/   \__, //_/    \____/ \__,_// .___/
                                 /____/                    /_/
   ========================================================================== */


void config_run_tests()
{
    mt_prepare_test = &test_prepare;

    mt_run(config_all_default);
#if SHELLDOWN_ENABLE_GETOPT || SHELLDOWN_ENABLE_GETOPT_LONG
    mt_run(config_short_opts);
#endif
#if SHELLDOWN_ENABLE_GETOPT_LONG
    mt_run(config_long_opts);
#endif
#if SHELLDOWN_ENABLE_INI
    mt_run(config_from_file);
#endif
    mt_run(config_print_help);
    mt_run(config_print_version);
}
