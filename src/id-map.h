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
 * he we would set src=shellyplus1pm-7c86cf65sd2c and dst=heat/office
 */
struct id_map
{
	char          *src;  /* shelly id which shall be renamed */
	char          *dst;  /* new shelly id, can contain '/' characters */
	struct id_map *next; /* pointer to na next id_map */
};

typedef struct id_map* id_map_t;

int id_map_init(id_map_t *head);
int id_map_add(id_map_t *head, const char *src, const char *dst);
const char *id_map_find(id_map_t head, const char *src);
int id_map_delete(id_map_t *head, const char *src);
int id_map_clear(id_map_t *head);
int id_map_print(id_map_t head);
int id_map_add_from_file(id_map_t *head, const char *file);

#define id_map_foreach(head) \
	for (id_map_t node = head; node != NULL; node = node->next)

#endif
