/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#ifndef __KNOT_THING_MAIN_H__
#define __KNOT_THING_MAIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "knot_thing_protocol.h"

typedef int (*intDataFunction)		(int32_t *val, int32_t *multiplier);
typedef int (*floatDataFunction)	(int32_t *val_int, uint32_t *val_dec, int32_t *multiplier);
typedef int (*boolDataFunction)		(uint8_t *val);

/* Return ammount read or written */
typedef int (*rawReadFunction)		(uint8_t *buffer, uint8_t len);
typedef int (*rawWriteFunction)		(const uint8_t *buffer, uint8_t len);

typedef struct __attribute__ ((packed)) {
	intDataFunction read;
	intDataFunction write;
} knot_int_functions;

typedef struct __attribute__ ((packed)) {
	floatDataFunction read;
	floatDataFunction write;
} knot_float_functions;

typedef struct __attribute__ ((packed)) {
	boolDataFunction read;
	boolDataFunction write;
} knot_bool_functions;

typedef struct __attribute__ ((packed)) {
	rawReadFunction read;
	rawWriteFunction write;
} knot_raw_functions;

typedef union __attribute__ ((packed)) {
	knot_int_functions	int_f;
	knot_float_functions	float_f;
	knot_bool_functions	bool_f;
	knot_raw_functions	raw_f;
} knot_data_functions;

/* KNOT Thing main initialization functions and polling */
int8_t	knot_thing_init(const char *thing_name);
void	knot_thing_exit(void);
int8_t	knot_thing_run(void);

/*
 * Data item (source/sink) registration functions
 *
 * The allowed values for type_id and unit parameters are defined by KNOT
 * protocol in knot_types.h file.
 *
 * https://github.com/CESARBR/knot-protocol-source/blob/master/src/knot_types.h
 */
int8_t knot_thing_register_raw_data_item(uint8_t sensor_id, const char *name,
	uint8_t *raw_buffer, uint8_t raw_buffer_len, uint16_t type_id,
	uint8_t value_type, uint8_t unit, knot_data_functions *func);

int8_t knot_thing_register_data_item(uint8_t sensor_id, const char *name, uint16_t type_id,
	uint8_t value_type, uint8_t unit, knot_data_functions *func);

/* Create schema for data item in position given by index if valid */
int knot_thing_create_schema(uint8_t index, knot_msg_schema *msg);
int knot_thing_data_item_read(uint8_t id, knot_msg_data *data);
int knot_thing_data_item_write(uint8_t id, knot_msg_data *data);
int knot_thing_verify_events(knot_msg_data *data);
int knot_thing_config_data_item(uint8_t id, uint8_t evflags, uint16_t time_sec,
						knot_value_type *lower,
						knot_value_type *upper);

/*
 * Auxiliary functions
 */

/* Find id for item in given index. Returns 0 if index is out of boundaries */
uint8_t knot_thing_get_sensor_id(const uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* __KNOT_THING_MAIN_H__ */
