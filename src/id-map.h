/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef SHELLDOWN_ID_MAP_H
#define SHELLDOWN_ID_MAP_H 1


/* shelly gen2 publishes its messages on "$shellydev-$id", ie:
 * shellyplus1pm-7c86cf65sd2c/#
 * In case user wants to set custom shelly id, he can do it via
 * id-map.
 *
 * So let's say user wants his shelly to be called heat/office,
 * he we would set from=shellyplus1pm-7c86cf65sd2c and to=heat/office
 */
struct id_map
{
	char          *from; /* shelly id which shall be renamed */
	char          *to;   /* new shelly id, can contain '/' characters */
	struct id_map *next; /* pointer to na next id_map */
};

typedef struct id_map* id_map_t;

int id_map_init(id_map_t *head);
int id_map_add(id_map_t *head, const char *from, const char *to);
int id_map_delete(id_map_t *head, const char *from);
int id_map_clear(id_map_t *head);
int id_map_print(id_map_t head);

#endif
