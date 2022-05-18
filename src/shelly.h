/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef SHELLDOWN_SHELLY_H
#define SHELLDOWN_SHELLY_H 1

#define json_object_get_or_error(m, v, r, k) \
	v = json_object_get(r, k); \
	if (v == NULL) \
		goto_print(error, ELW, "["m"] no "k" in json: %s", payload)

int shelly_id_to_ver(const char *id);

#define declare_shelly(s) \
	void shelly_##s##_pub(const char *topic, const char *payload, int qos, int retain); \
	void shelly_##s##_set(const char *topic, const char *payload, int qos, int retain)

declare_shelly(plus1pm);

#endif
