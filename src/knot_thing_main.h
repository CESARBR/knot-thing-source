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
typedef int (*rawDataFunction)		(uint8_t *val, uint8_t *len);

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
	rawDataFunction read;
	rawDataFunction write;
} knot_raw_functions;

typedef union __attribute__ ((packed)) {
	knot_int_functions	int_f;
	knot_float_functions	float_f;
	knot_bool_functions	bool_f;
	knot_raw_functions	raw_f;
} knot_data_functions;

/* KNOT Thing main initialization functions and polling */
int8_t	knot_thing_init(const int protocol, const char *thing_name);
void	knot_thing_exit(void);
int8_t	knot_thing_run(void);

/*
 * Data item (source/sink) registration/configuration functions
 */
int8_t knot_thing_register_raw_data_item(uint8_t sensor_id, const char *name,
	uint8_t *raw_buffer, uint8_t raw_buffer_len, uint16_t type_id,
	uint8_t value_type, uint8_t unit, knot_data_functions *func);

int8_t knot_thing_register_data_item(uint8_t sensor_id, const char *name, uint16_t type_id,
	uint8_t value_type, uint8_t unit, knot_data_functions *func);

int8_t knot_thing_config_data_item(uint8_t sensor_id, uint8_t event_flags,
	knot_value_types *lower_limit, knot_value_types *upper_limit);

#ifdef __cplusplus
}
#endif

#endif /* __KNOT_THING_MAIN_H__ */
