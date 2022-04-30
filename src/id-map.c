/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         ------------------------------------------------------------
        / id-map - a very minimal linked list implementation         \
        \ that holds from->to map                                    /
         ------------------------------------------------------------
          \
           \ ,   _ ___.--'''`--''//-,-_--_.
              \`"' ` || \\ \ \\/ / // / ,-\\`,_
             /'`  \ \ || Y  | \|/ / // / - |__ `-,
            /@"\  ` \ `\ |  | ||/ // | \/  \  `-._`-,_.,
           /  _.-. `.-\,___/\ _/|_/_\_\/|_/ |     `-._._)
           `-'``/  /  |  // \__/\__  /  \__/ \
                `-'  /-\/  | -|   \__ \   |-' |
                  __/\ / _/ \/ __,-'   ) ,' _|'
                 (((__/(((_.' ((___..-'((__,'
   ==========================================================================
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/
   ========================================================================== */
#include "id-map.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <embedlog.h>
#include <ctype.h>

#include "macros.h"


/* ==========================================================================
                     ____   _____ (_)_   __ ____ _ / /_ ___
                    / __ \ / ___// /| | / // __ `// __// _ \
                   / /_/ // /   / / | |/ // /_/ // /_ /  __/
                  / .___//_/   /_/  |___/ \__,_/ \__/ \___/
                 /_/
   ==========================================================================
    Finds a node that contains 'topic' string.

    Returns pointer to found node or null.

    If 'prev' is not null, function will also return previous node that
    'next' field points to returned node. This is usefull when deleting
    nodes and saves searching for previous node to rearange list after
    delete. If function returns valid pointer, and prev is NULL, this
    means first element from the list was returned.
   ========================================================================== */
static id_map_t id_map_find_node
(
	id_map_t         head,  /* head of the list to search */
	const char      *from,  /* from shelly id to look for */
	id_map_t        *prev   /* previous node to returned node */
)
{
	id_map_t         node;  /* current node */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	for (*prev = NULL, node = head; node != NULL; node = node->next)
	{
		if (strcmp(node->from, from) == 0)
			return node; /* this is the node you are looking for */

		*prev = node;
	}

	/* node of that topic does not exist */
	return NULL;
}


/* ==========================================================================
    Creates new node with copy of $from and $to

    Returns NULL on error or address on success

    errno:
            ENOMEM      not enough memory for new node
   ========================================================================== */
static id_map_t id_map_new_node
(
	const char      *from,  /* from id to create new node with */
	const char      *to     /* to id to create new node with */
)
{
	id_map_t        node;  /* pointer to new node */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	/* allocate enough memory for strings (plus 1 for null character)
	 * and node in one malloc(), this way we will have only 1
	 * allocation (for node and string) instead of 2.  */
	node = malloc(sizeof(struct id_map) + strlen(from)+1 + strlen(to)+1);
	if (node == NULL)
		return NULL;

	/* we have flat memory allocated, now we need
	 * to set list member to point to correct memory */
	node->from = ((char *)node) + sizeof(struct id_map);
	node->to   = node->from + strlen(from)+1;

	/* make a copy of members */
	strcpy(node->from, from);
	strcpy(node->to, to);

	/* since this is new node, it
	 * doesn't point to anything */
	node->next = NULL;

	return node;
}


/* ==========================================================================
                                        __     __ _
                         ____   __  __ / /_   / /(_)_____
                        / __ \ / / / // __ \ / // // ___/
                       / /_/ // /_/ // /_/ // // // /__
                      / .___/ \__,_//_.___//_//_/ \___/
                     /_/
   ==========================================================================
    Initializes new list. Just makes sure head is NULL and not wild pointer,
    which would lead to segfault.
   ========================================================================== */
int id_map_init
(
	id_map_t  *head  /* list to initialize */
)
{
	*head = NULL;
	return 0;
}

/* ==========================================================================
    Adds new node with $from and $to to list pointed by 'head'

    Function will add node just after $head, not at the end of list - this is
    so we can gain some speed by not searching for last node. So when $head
    list is

        +---+     +---+
        | 1 | --> | 2 |
        +---+     +---+

    And node '3' is added, list will be

        +---+     +---+     +---+
        | 1 | --> | 3 | --> | 2 |
        +---+     +---+     +---+

    If $head is NULL (meaning list is empty), function will create new list
    and add $from and $to to $head
   ========================================================================== */
int id_map_add
(
	id_map_t    *head,  /* head of list where to add new node to */
	const char  *from,  /* shelly from string */
	const char  *to     /* shelly to string */
)
{
	id_map_t     node;  /* newly created node */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	valid(head, EINVAL);
	valid(from, EINVAL);
	valid(to, EINVAL);

	/* create new node, let's call it 3
	 *           +---+
	 *           | 3 |
	 *           +---+
	 *
	 *      +---+     +---+
	 *      | 1 | --> | 2 |
	 *      +---+     +---+ */
	node = id_map_new_node(from, to);
	if (node == NULL)
		return -1;

	if (*head == NULL)
	{
		/* head is null, so list is empty and we
		 * are creating new list here. In that
		 * case, simply set *head with newly
		 * created node and exit */
		*head = node;
		return 0;
	}

	/* set new node's next field, to second item
	 * in the list, if there is no second item, it
	 * will point to NULL
	 *           +---+
	 *           | 3 |
	 *           +---+.
	 *                 \
	 *                 V
	 *      +---+     +---+
	 *      | 1 | --> | 2 |
	 *      +---+     +---+ */
	node->next = (*head)->next;

	/* set head's next to point to newly created
	 * node so our list is complete once again.
	 *           +---+
	 *           | 3 |
	 *           +---+.
	 *           ^     \
	 *          /      V
	 *      +---+     +---+
	 *      | 1 |     | 2 |
	 *      +---+     +---+ */
	(*head)->next = node;

	return 0;
}


/* ==========================================================================
    Removes $from from list $head. This will remove whole list object, so
    corresponding $to (in node object) will be removed as well. $from
    is a uniq key, so we look only for this when deleting.

    - if $from is in $head node, function will modify $head pointer
      so $head points to proper node

    - if $topic is in $head node and $head turns out to be last element
      int the list, $head will become NULL
   ========================================================================== */
int id_map_delete
(
	id_map_t    *head,        /* pointer to head of the list */
	const char  *from         /* node with that shelly from to delete */
)
{
	id_map_t     node;       /* found node for with 'topic' */
	id_map_t     prev_node;  /* previous node of found 'node' */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	valid(from, EINVAL);
	valid(head, EINVAL);
	valid(*head, ENOENT);

	node = id_map_find_node(*head, from, &prev_node);
	if (node == NULL)
		return_errno(ENOENT); /* node with $from does not exist */

	/* initial state of the list
	 *            +---+     +---+     +---+
	 *   head --> | 1 | --> | 3 | --> | 2 |
	 *            +---+     +---+     +---+ */
	if (node == *head)
	{
		/* caller wants to delete node that is
		 * currently head node, so we need to
		 * remove head, and them make head->next
		 * new head of the list
		 *
		 *            +---+          +---+     +---+
		 *   node --> | 1 | head --> | 3 | --> | 2 |
		 *            +---+          +---+     +---+ */
		*head = node->next;

		/* now '1' is detached from anything and
		 * can be safely freed */
		free(node);
		return 0;
	}

	/* node points to something else than 'head'
	 *
	 *            +---+     +---+     +---+     +---+
	 *   head --> | 1 | --> | 3 | --> | 2 | --> | 4 |
	 *            +---+     +---+     +---+     +---+
	 *                                 ^
	 *                                 |
	 *                                node
	 *
	 * before deleting, we need to make sure '3'
	 * (prev node) points to '4'. If node points
	 * to last element '4', then we will set next
	 * member of '2' element to null.
	 *
	 *            +---+     +---+     +---+
	 *   head --> | 1 | --> | 3 | --> | 4 |
	 *            +---+     +---+     +---+
	 *                                 ^
	 *                      +---+     /
	 *             node --> | 2 | ---`
	 *                      +---+ */
	prev_node->next = node->next;

	/* now that list is consistent again, we can
	 * remove node (2) without destroying list */
	free(node);
	return 0;
}


/* ==========================================================================
    Removes all elements in the list pointed by $head. This also free(3) all
    allocated memory by id_map.
   ========================================================================== */
int id_map_clear
(
	id_map_t  *head  /* list to clear */
)
{
	id_map_t   next;  /* next node to free(3) */
	id_map_t   node;  /* current node to free(3) */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if (head == NULL)
		return 0; /* list is already empty */

	for (node = *head ; node != NULL; node = next)
	{
		/* cache next, it will not be
		 * available after free */
		next = node->next;
		free(node);
	}

	/* nullify callers variable, so it's safe to call
	 * id_map_new() right after clearning. */
	*head = NULL;
	return 0;
}


/* ==========================================================================
    Prints contents of id_map.
   ========================================================================== */
int id_map_print
(
	id_map_t head
)
{
	if (head == NULL)
		return_print(0, 0, ELN, "id map id empty");

	el_print(ELN, "contents of id map:");
	for (; head != NULL; head = head->next)
		el_print(ELN, "    %s -> %s", head->from, head->to);

	return 0;
}


/* ==========================================================================
    Reads $file and adds map values from it to $head. File can contain
    comments (#), and all whitespaces are ignored, so padding is allowed.
   ========================================================================== */
int id_map_add_from_file
(
	id_map_t    *head,     /* list to append map from file */
	const char  *file      /* file to read map from */
)
{
	FILE        *f;        /* pointer to opened file */
	int         lineno;    /* current line number */
	char       *from;      /* shelly from read from a file */
	char       *to;        /* shelly from read from a file */
	char        line[256]; /* line read from a file */
	char       *linep;     /* pointer to a line[], for easy manipulation */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if ((f = fopen(file, "r")) == NULL)
		return_perror(ELC, "fopen(%s)", file);

	for (lineno = 1;; lineno++)
	{
		/* set last byte of line buffer to something other than
		 * '\0' to know whether fgets have overwritten it or not */
		line[sizeof(line) - 1] = 0xaa;

		/* try to read whole line into buffer */
		if (fgets(line, sizeof(line), f) == NULL)
		{
			/* failed to read anything from a file, whatever happens
			 * now, we close file as it's no longer useable */
			fclose(f);

			if (feof(f))
				return 0; /* end of file, we parsed it all */

			/* error reading file */
			return_perror(ELC, "fgets(%s)", file);
		}

		/* fgets overwritted last byte with '\0', which means
		 * it filled whole line buffer with data
		 *   -- and --
		 * last character in string is not a new line
		 * character, so our line buffer turns out to be too
		 * small and we couln't read whole line into buffer. */
		if (line[sizeof(line) - 1] == '\0' && line[sizeof(line) - 2] != '\n')
			continue_print(ELE, "[%s:%d] line to long, ignoring", file, lineno);

		/* line is empty (only new line character is present
		 *   -- or --
		 * line is a comment (starting from #) */
		if (line[0] == '\n' || line[0] == '#')
			continue;


		/* we have full line read, let's parse it */
		linep = line;


		/* skip leading whitespaces */
		while (*linep != '\0' && isspace(*linep)) linep++;
		if (*linep == '\0') continue; /* empty line */

		/* linep points to start of a shelly from part */
		from = linep;
		/* look for a whitespace, which is a delimiter */
		while (*linep != '\0' && !isspace(*linep)) linep++;
		if (*linep == '\0')
			continue_print(ELC, "[%s:%d] missing 'to' part", file, lineno);
		/* linep is pointing to a whitespace, replace that with
		 * '\0', so *from is a valid c-string with shelly from */
		*linep++ = '\0';

		/* now let's find "to" part of map */
		/* again, skip leading whitespaces */
		while (*linep != '\0' && isspace(*linep)) linep++;
		if (*linep == '\0')
			continue_print(ELC, "[%s:%d] missing 'to' part", file, lineno);
		/* found "to" part */
		to = linep;
		/* let's find first whitespace, in case user left some
		 * whitespaces at the end of file, we are guaranteed to
		 * at least find '\n' */
		while (!isspace(*linep)) linep++;
		/* nullify it to get valid c-string in $to */
		*linep++ = '\0';

		/* finnaly, let's add read map to a database */
		if (id_map_add(head, from, to))
			return_perror(ELF, "id_map_add(%s, %s)", from, to);

		/* and that concludes parsing of this line,
		 * move out to the next one */
		continue;
	}

	/* all lines parsed */
	return 0;
}
