/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */
#ifndef BOFC_MACROS_H
#define BOFC_MACROS_H 1

#include <errno.h>

/* macros version 0.2.0 */

#define return_print(r, e, ...) { el_print(__VA_ARGS__); errno = e; return r; }

#define return_errno(e)     { errno = e; return -1; }
#define return_perror(...)  { el_perror(__VA_ARGS__); return -1; }

#define goto_perror(l, ...) { el_perror(__VA_ARGS__); goto l; }

#define continue_print(...) { el_print(__VA_ARGS__); continue; }
#define continue_perror(...){ el_perror(__VA_ARGS__); continue; }
#define break_print(...)    { el_print(__VA_ARGS__); break; }


/* macros to validate function input arguments */

/* ==========================================================================
    If expression $x evaluates to false,  macro will set errno value to $e
    and will force function to return with code '-1'
   ========================================================================== */
#define valid(x, e) if (!(x)) return_errno(e)


/* ==========================================================================
    Same as valid but when expression fails, message is printed to default
    embedlog facility
   ========================================================================== */
#define valid_print(x, e) if (!(x)) return_print(-1, e, ELW, "valid failed "#x)


/* ==========================================================================
    Same as valid_print but message is printed to default option, defined by
    EL_OPTIONS_OBJECT
   ========================================================================== */
#define valid_oprint(x, e) if (!(x)) return_print(-1, e, OELW, "valid failed "#x)


/* ==========================================================================
    If expression $x evaluates to false, macro will set errno value to $e
    and will jump to label $l
   ========================================================================== */
#define valid_goto(x, l, e) if (!(x)) { errno = e; goto l; }


#endif
