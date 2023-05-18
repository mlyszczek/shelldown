/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef SHELLDOWN_ID_MAP_H
#define SHELLDOWN_ID_MAP_H 1


/* Generic id map for shellies.
 *
 * ---
 * shelly gen2 publishes its messages on "$shellydev-$id", ie:
 * shellyplus1pm-7c86cf65sd2c/#
 * In case user wants to set custom shelly id, he can do it via
 * id-map.
 *
 * So let's say user wants his shelly to be called heat/office,
 * he we would set src=shellyplus1pm-7c86cf65sd2c and dst=heat/office
 *
 * ---
 * shelly i4 has 2 modes. switch and button. But esentiall they work in
 * the same way - just send different data over mqtt. But they both will
 * report button down and then button up (input on, input off). That does
 * not really makes sens for user. To make more sense, when i4 is in
 * switch mode - it just sends 1 or 0 exactly as it is on the input.
 * But for button we will send "1" for first down/up event, and second
 * click will send 0.
 */
struct id_map
{
	char          *src;  /* shelly id which shall be renamed */
	union
	{
		char      *dst;  /* new shelly id, can contain '/' characters */
		int        state;/* for shelly i4, represents button state */
	};
	struct id_map *next; /* pointer to na next id_map */
};

typedef struct id_map* id_map_t;

int id_map_init(id_map_t *head);

int id_map_add_dst(id_map_t *head, const char *src, const char *dst);
int id_map_add_dst_from_file(id_map_t *head, const char *file);
const char *id_map_find_dst(id_map_t head, const char *src);

int id_map_add_state(id_map_t *head, const char *src, int);

int id_map_delete(id_map_t *head, const char *src);
int id_map_clear(id_map_t *head);
int id_map_print(id_map_t head);
id_map_t id_map_find_node(id_map_t head, const char *src, id_map_t *prev);

#define id_map_foreach(head) \
	for (id_map_t node = head; node != NULL; node = node->next)

#endif
