/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#ifndef __KNOT_THING_PROTOCOL_H__
#define __KNOT_THING_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "knot_protocol.h"

typedef int (*data_function)(uint8_t sensor_id, knot_data *data);
typedef int (*schema_function)(uint8_t sensor_id, knot_msg_schema *schema);
typedef int (*config_function)(uint8_t sensor_id, uint8_t event_flags,
		knot_value_types *lower_limit, knot_value_types *upper_limit);

int knot_thing_protocol_init(uint8_t protocol, const char *thing_name,
				data_function read, data_function write,
				schema_function schema, config_function config);
void knot_thing_protocol_exit(void);
int knot_thing_protocol_run(void);


#ifdef __cplusplus
}
#endif

#endif /* __KNOT_THING_PROTOCOL_H__ */
