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
	knot_value_types	last_data;
	uint8_t			*last_value_raw;
	// config values
	knot_config		config;	// Flags indicating when data will be sent
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
		data_items[index].config.event_flags			= KNOT_EVT_FLAG_UNREGISTERED;

		data_items[index].last_data.val_f.multiplier		= 1; // as "last_data" is a union, we need just to
		data_items[index].last_data.val_f.value_int		= 0; // set the "biggest" member
		data_items[index].last_data.val_f.value_dec		= 0;
		data_items[index].config.lower_limit.val_f.multiplier	= 1; // as "lower_limit" is a union, we need just to
		data_items[index].config.lower_limit.val_f.value_int	= 0; // set the "biggest" member
		data_items[index].config.lower_limit.val_f.value_dec	= 0;
		data_items[index].config.upper_limit.val_f.multiplier	= 1; // as "upper_limit" is a union, we need just to
		data_items[index].config.upper_limit.val_f.value_int	= 0; // set the "biggest" member
		data_items[index].config.upper_limit.val_f.value_dec	= 0;
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
	return (!(data_items[sensor_id].config.event_flags & KNOT_EVT_FLAG_UNREGISTERED));
}

int8_t knot_thing_init(const int protocol, const char *thing_name)
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

	data_items[sensor_id].name					= name;
	data_items[sensor_id].type_id					= type_id;
	data_items[sensor_id].unit					= unit;
	data_items[sensor_id].value_type				= value_type;
	// TODO: load flags and limits from persistent storage
	data_items[sensor_id].config.event_flags			= KNOT_EVT_FLAG_NONE; // remove KNOT_EVT_FLAG_UNREGISTERED flag
	data_items[sensor_id].last_data.val_f.multiplier		= 1; // as "last_data" is a union, we need just to
	data_items[sensor_id].last_data.val_f.value_int			= 0; // set the "biggest" member
	data_items[sensor_id].last_data.val_f.value_dec			= 0;
	data_items[sensor_id].config.lower_limit.val_f.multiplier	= 1; // as "lower_limit" is a union, we need just to
	data_items[sensor_id].config.lower_limit.val_f.value_int	= 0; // set the "biggest" member
	data_items[sensor_id].config.lower_limit.val_f.value_dec	= 0;
	data_items[sensor_id].config.upper_limit.val_f.multiplier	= 1; // as "upper_limit" is a union, we need just to
	data_items[sensor_id].config.upper_limit.val_f.value_int	= 0; // set the "biggest" member
	data_items[sensor_id].config.upper_limit.val_f.value_dec	= 0;
	data_items[sensor_id].last_value_raw				= NULL;

	data_items[sensor_id].functions.int_f.read			= func->int_f.read; // as "functions" is a union, we need just to
	data_items[sensor_id].functions.int_f.write			= func->int_f.write; // set only one of its members
	return 0;
}

int8_t knot_thing_config_data_item(uint8_t sensor_id, uint8_t event_flags,
	knot_value_types *lower_limit, knot_value_types *upper_limit)
{
	if ((sensor_id >= KNOT_THING_DATA_MAX) || item_is_unregistered(sensor_id) == 0)
		return -1;

	data_items[sensor_id].config.event_flags = event_flags;
	if (lower_limit != NULL)
	{
		data_items[sensor_id].config.lower_limit.val_f.multiplier	= lower_limit->val_f.multiplier; // as "lower_limit" is a union, we need just to
		data_items[sensor_id].config.lower_limit.val_f.value_int	= lower_limit->val_f.value_int;  // set the "biggest" member
		data_items[sensor_id].config.lower_limit.val_f.value_dec	= lower_limit->val_f.value_dec;
	}

	if (upper_limit != NULL)
	{
		data_items[sensor_id].config.upper_limit.val_f.multiplier	= upper_limit->val_f.multiplier; // as "upper_limit" is a union, we need just to
		data_items[sensor_id].config.upper_limit.val_f.value_int	= upper_limit->val_f.value_int;  // set the "biggest" member
		data_items[sensor_id].config.upper_limit.val_f.value_dec	= upper_limit->val_f.value_dec;
	}
	// TODO: store flags and limits on persistent storage
	return 0;
}

int8_t knot_thing_run(void)
{
	uint8_t i = 0, uint8_val = 0, comparison = 0, uint8_buffer[KNOT_DATA_RAW_SIZE];
	int32_t int32_val = 0, multiplier = 0;
	uint32_t uint32_val = 0;
	/* TODO: Monitor messages from network */

	/*
	 * For all registered data items: verify if value
	 * changed according to the events registered.
	 */

	// TODO: add timer events
	for (i = 0; i < KNOT_THING_DATA_MAX; i++)
	{
		if (data_items[i].value_type == KNOT_VALUE_TYPE_RAW)
		{
			// Raw supports only KNOT_EVT_FLAG_CHANGE
			if ((KNOT_EVT_FLAG_CHANGE & data_items[i].config.event_flags) == 0)
				continue;

			if (data_items[i].functions.raw_f.read == NULL)
				continue;

			if (data_items[i].last_value_raw == NULL)
				continue;

			if (data_items[i].functions.raw_f.read(&uint8_val, uint8_buffer) < 0)
				continue;

			if (uint8_val != KNOT_DATA_RAW_SIZE)
				continue;

			if (memcmp(data_items[i].last_value_raw, uint8_buffer, KNOT_DATA_RAW_SIZE) == 0)
				continue;

			memcpy(data_items[i].last_value_raw, uint8_buffer, KNOT_DATA_RAW_SIZE);
			// TODO: Send message (as raw is not in last_data structure)
			continue; // Raw actions end here
		}
		else if (data_items[i].value_type == KNOT_VALUE_TYPE_BOOL)
		{
			if (data_items[i].functions.bool_f.read == NULL)
				continue;

			if (data_items[i].functions.bool_f.read(&uint8_val) < 0)
				continue;

			if (uint8_val != data_items[i].last_data.val_b)
			{
				comparison |= (KNOT_EVT_FLAG_CHANGE & data_items[i].config.event_flags);
				data_items[i].last_data.val_b = uint8_val;
			}

		}
		else if (data_items[i].value_type == KNOT_VALUE_TYPE_INT)
		{
			if (data_items[i].functions.int_f.read == NULL)
				continue;

			if (data_items[i].functions.int_f.read(&int32_val, &multiplier) < 0)
				continue;

			// TODO: add multiplier to comparison
			if (int32_val < data_items[i].config.lower_limit.val_i.value)
				comparison |= (KNOT_EVT_FLAG_LOWER_THRESHOLD & data_items[i].config.event_flags);
			else if (int32_val > data_items[i].config.upper_limit.val_i.value)
				comparison |= (KNOT_EVT_FLAG_UPPER_THRESHOLD & data_items[i].config.event_flags);
			if (int32_val != data_items[i].last_data.val_i.value)
				comparison |= (KNOT_EVT_FLAG_CHANGE & data_items[i].config.event_flags);

			data_items[i].last_data.val_i.value = int32_val;
			data_items[i].last_data.val_i.multiplier = multiplier;
		}
		else if (data_items[i].value_type == KNOT_VALUE_TYPE_FLOAT)
		{
			if (data_items[i].functions.float_f.read == NULL)
				continue;

			if (data_items[i].functions.float_f.read(&int32_val, &uint32_val, &multiplier) < 0)
				continue;

			// TODO: add multiplier and decimal part to comparison
			if (int32_val < data_items[i].config.lower_limit.val_f.value_int)
				comparison |= (KNOT_EVT_FLAG_LOWER_THRESHOLD & data_items[i].config.event_flags);
			else if (int32_val > data_items[i].config.upper_limit.val_f.value_int)
				comparison |= (KNOT_EVT_FLAG_UPPER_THRESHOLD & data_items[i].config.event_flags);
			if (int32_val != data_items[i].last_data.val_f.value_int)
				comparison |= (KNOT_EVT_FLAG_CHANGE & data_items[i].config.event_flags);

			data_items[i].last_data.val_f.value_int = int32_val;
			data_items[i].last_data.val_f.value_dec = uint32_val;
			data_items[i].last_data.val_f.multiplier = multiplier;
		}
		else // This data item is not registered with a valid value type
		{
			continue;
		}

		// Nothing changed
		if (comparison == 0)
			continue;

		// TODO: If something changed, send message
	}

	return 0;
}
