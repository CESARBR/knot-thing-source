/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "knot_thing_config.h"
#include "knot_types.h"
#include "knot_thing_main.h"

// TODO: normalize all returning error codes

#define KNOT_THING_EMPTY_ITEM		"EMPTY ITEM"

static struct {
	// schema values
	uint8_t			value_type;	// KNOT_VALUE_TYPE_* (int, float, bool, raw)
	uint8_t			unit;		// KNOT_UNIT_*
	uint16_t		type_id;	// KNOT_TYPE_ID_*
	const char		*name;		// App defined data item name
	// data values
	knot_data_values	last_data;
	uint8_t			*last_value_raw;
	// config values
	uint8_t			event_flags;	// Flags indicating when data will be sent
	knot_data_values	lower_limit;
	knot_data_values	upper_limit;
	// Data read/write functions
	knot_data_functions	functions;
} data_items[KNOT_THING_DATA_MAX];

static void reset_data_items(void)
{
	int8_t index = 0;

	for (index = 0; index < KNOT_THING_DATA_MAX; index++)
	{
		data_items[index].name					= KNOT_THING_EMPTY_ITEM;
		data_items[index].type_id				= KNOT_TYPE_ID_INVALID;
		data_items[index].unit					= KNOT_UNIT_NOT_APPLICABLE;
		data_items[index].value_type				= KNOT_VALUE_TYPE_INVALID;
		data_items[index].event_flags				= KNOT_EVT_FLAG_UNREGISTERED;

		data_items[index].last_data.value_f.multiplier		= 1; // as "last_data" is a union, we need just to
		data_items[index].last_data.value_f.value_int		= 0; // set the "biggest" member
		data_items[index].last_data.value_f.value_dec		= 0;
		data_items[index].lower_limit.value_f.multiplier	= 1; // as "lower_limit" is a union, we need just to
		data_items[index].lower_limit.value_f.value_int		= 0; // set the "biggest" member
		data_items[index].lower_limit.value_f.value_dec		= 0;
		data_items[index].upper_limit.value_f.multiplier	= 1; // as "upper_limit" is a union, we need just to
		data_items[index].upper_limit.value_f.value_int		= 0; // set the "biggest" member
		data_items[index].upper_limit.value_f.value_dec		= 0;
		data_items[index].last_value_raw			= NULL;

		data_items[index].functions.int_f.read	= NULL; // as "functions" is a union, we need just to
		data_items[index].functions.int_f.write	= NULL; // set only one of its members
	}
}

int data_function_is_valid(knot_data_functions *func)
{
	if (func == NULL)
		return -1;
		
	if (func->int_f.read == NULL && func->int_f.write == NULL)
		return -1;

	return 0;
}

uint8_t item_is_unregistered(uint8_t sensor_id)
{
	return (!(data_items[sensor_id].event_flags & KNOT_EVT_FLAG_UNREGISTERED));
}

int8_t knot_thing_init(void)
{
	reset_data_items();
	return 0;
}

void knot_thing_exit(void)
{

}

int8_t knot_thing_register_raw_data_item(uint8_t sensor_id, const char *name,
	uint8_t *raw_buffer, uint8_t raw_buffer_len, uint16_t type_id,
	uint8_t value_type, uint8_t unit, knot_data_functions *func)
{
	if (raw_buffer == NULL)
		return -1;

	if (raw_buffer_len != KNOT_DATA_RAW_SIZE)
		return -1;

	if (knot_thing_register_data_item(sensor_id, name, type_id, value_type, 
		unit, func) != 0)
		return -1;

	data_items[sensor_id].last_value_raw	= raw_buffer;
	return 0;
}


int8_t knot_thing_register_data_item(uint8_t sensor_id, const char *name,
	uint16_t type_id, uint8_t value_type, uint8_t unit,
	knot_data_functions *func)
{
	if (sensor_id >= KNOT_THING_DATA_MAX)
		return -1;

	if ((item_is_unregistered(sensor_id) != 0) || 
		(knot_schema_is_valid(type_id, value_type, unit) != 0) ||
		name == NULL || (data_function_is_valid(func) != 0))
		return -1;

	data_items[sensor_id].name				= name;
	data_items[sensor_id].type_id				= type_id;
	data_items[sensor_id].unit				= unit;
	data_items[sensor_id].value_type			= value_type;
	// TODO: load flags and limits from persistent storage
	data_items[sensor_id].event_flags			= KNOT_EVT_FLAG_NONE; // remove KNOT_EVT_FLAG_UNREGISTERED flag
	data_items[sensor_id].last_data.value_f.multiplier	= 1; // as "last_data" is a union, we need just to
	data_items[sensor_id].last_data.value_f.value_int	= 0; // set the "biggest" member
	data_items[sensor_id].last_data.value_f.value_dec	= 0;
	data_items[sensor_id].lower_limit.value_f.multiplier	= 1; // as "lower_limit" is a union, we need just to
	data_items[sensor_id].lower_limit.value_f.value_int	= 0; // set the "biggest" member
	data_items[sensor_id].lower_limit.value_f.value_dec	= 0;
	data_items[sensor_id].upper_limit.value_f.multiplier	= 1; // as "upper_limit" is a union, we need just to
	data_items[sensor_id].upper_limit.value_f.value_int	= 0; // set the "biggest" member
	data_items[sensor_id].upper_limit.value_f.value_dec	= 0;
	data_items[sensor_id].last_value_raw			= NULL;

	data_items[sensor_id].functions.int_f.read		= func->int_f.read; // as "functions" is a union, we need just to
	data_items[sensor_id].functions.int_f.write		= func->int_f.write; // set only one of its members
	return 0;
}

int8_t knot_thing_config_data_item(uint8_t sensor_id, uint8_t event_flags,
	knot_data_values *lower_limit, knot_data_values *upper_limit)
{
	if ((sensor_id >= KNOT_THING_DATA_MAX) || item_is_unregistered(sensor_id) == 0)
		return -1;

	data_items[sensor_id].event_flags = event_flags;
	if (lower_limit != NULL)
	{
		data_items[sensor_id].lower_limit.value_f.multiplier	= lower_limit->value_f.multiplier; // as "lower_limit" is a union, we need just to
		data_items[sensor_id].lower_limit.value_f.value_int	= lower_limit->value_f.value_int;  // set the "biggest" member
		data_items[sensor_id].lower_limit.value_f.value_dec	= lower_limit->value_f.value_dec;
	}

	if (upper_limit != NULL)
	{
		data_items[sensor_id].upper_limit.value_f.multiplier	= upper_limit->value_f.multiplier; // as "upper_limit" is a union, we need just to
		data_items[sensor_id].upper_limit.value_f.value_int	= upper_limit->value_f.value_int;  // set the "biggest" member
		data_items[sensor_id].upper_limit.value_f.value_dec	= upper_limit->value_f.value_dec;
	}
	// TODO: store flags and limits on persistent storage
	return 0;
}

int8_t knot_thing_run(void)
{
	/* TODO: Monitor messages from network */

	/*
	 * TODO: For all registered data items: verify if value
	 * changed according to the events registered.
	 */


	return 0;
}
