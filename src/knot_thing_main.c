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

#include "include/time.h"
#include "knot_thing_config.h"
#include "knot_types.h"
#include "knot_thing_main.h"

// TODO: normalize all returning error codes

#define KNOT_THING_EMPTY_ITEM		"EMPTY ITEM"

/* Keeps track of max data_items index were there is a sensor/actuator stored */
static uint8_t max_sensor_id;
static uint8_t evt_sensor_id;

static struct _data_items{
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
	// time values
	uint32_t		last_timeout;	// Stores the last time the data was sent
	// Data read/write functions
	knot_data_functions	functions;
} data_items[KNOT_THING_DATA_MAX];

static void reset_data_items(void)
{
	int8_t count;
	max_sensor_id = 0;
	evt_sensor_id = 0;
	struct _data_items *pdata = data_items;

	for (count = 0; count < KNOT_THING_DATA_MAX; ++count, ++pdata) {
		pdata->name					= KNOT_THING_EMPTY_ITEM;
		pdata->type_id					= KNOT_TYPE_ID_INVALID;
		pdata->unit					= KNOT_UNIT_NOT_APPLICABLE;
		pdata->value_type				= KNOT_VALUE_TYPE_INVALID;
		pdata->config.event_flags			= KNOT_EVT_FLAG_UNREGISTERED;
		/* As "last_data" is a union, we need just to set the "biggest" member*/
		pdata->last_data.val_f.multiplier		= 1;
		pdata->last_data.val_f.value_int		= 0;
		pdata->last_data.val_f.value_dec		= 0;
		/* As "lower_limit" is a union, we need just to set the "biggest" member */
		pdata->config.lower_limit.val_f.multiplier	= 1;
		pdata->config.lower_limit.val_f.value_int	= 0;
		pdata->config.lower_limit.val_f.value_dec	= 0;
		/* As "upper_limit" is a union, we need just to set the "biggest" member */
		pdata->config.upper_limit.val_f.multiplier	= 1;
		pdata->config.upper_limit.val_f.value_int	= 0;
		pdata->config.upper_limit.val_f.value_dec	= 0;
		pdata->last_value_raw				= NULL;
		/* As "functions" is a union, we need just to set only one of its members */
		pdata->functions.int_f.read			= NULL;
		pdata->functions.int_f.write			= NULL;
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
	if (sensor_id >= KNOT_THING_DATA_MAX || (item_is_unregistered(sensor_id) != 0) ||
		(knot_schema_is_valid(type_id, value_type, unit) != 0) ||
		name == NULL || (data_function_is_valid(func) != 0))
		return -1;

	data_items[sensor_id].name					= name;
	data_items[sensor_id].type_id					= type_id;
	data_items[sensor_id].unit					= unit;
	data_items[sensor_id].value_type				= value_type;
	// TODO: load flags and limits from persistent storage
	/* Remove KNOT_EVT_FLAG_UNREGISTERED flag */
	data_items[sensor_id].config.event_flags			= KNOT_EVT_FLAG_NONE;
	/* As "last_data" is a union, we need just to set the "biggest" member */
	data_items[sensor_id].last_data.val_f.multiplier		= 1;
	data_items[sensor_id].last_data.val_f.value_int			= 0;
	data_items[sensor_id].last_data.val_f.value_dec			= 0;
	/* As "lower_limit" is a union, we need just to set the "biggest" member */
	data_items[sensor_id].config.lower_limit.val_f.multiplier	= 1;
	data_items[sensor_id].config.lower_limit.val_f.value_int	= 0;
	data_items[sensor_id].config.lower_limit.val_f.value_dec	= 0;
	/* As "upper_limit" is a union, we need just to set the "biggest" member */
	data_items[sensor_id].config.upper_limit.val_f.multiplier	= 1;
	data_items[sensor_id].config.upper_limit.val_f.value_int	= 0;
	data_items[sensor_id].config.upper_limit.val_f.value_dec	= 0;
	data_items[sensor_id].last_value_raw				= NULL;
	/* As "functions" is a union, we need just to set only one of its members */
	data_items[sensor_id].functions.int_f.read			= func->int_f.read;
	data_items[sensor_id].functions.int_f.write			= func->int_f.write;

	if (sensor_id > max_sensor_id)
		max_sensor_id = sensor_id;

	return 0;
}

int knot_thing_config_data_item(uint8_t sensor_id, uint8_t event_flags,
	uint16_t time_sec, knot_value_types *lower_limit, knot_value_types *upper_limit)
{
	/*FIXME: Check if config is valid */
	if ((sensor_id >= KNOT_THING_DATA_MAX) || item_is_unregistered(sensor_id) == 0)
		return -1;

	data_items[sensor_id].config.event_flags = event_flags;
	data_items[sensor_id].config.time_sec = time_sec;

	if (lower_limit != NULL) {
		/* As "lower_limit" is a union, we need just to set the "biggest" member */
		data_items[sensor_id].config.lower_limit.val_f.multiplier	= lower_limit->val_f.multiplier;
		data_items[sensor_id].config.lower_limit.val_f.value_int	= lower_limit->val_f.value_int;
		data_items[sensor_id].config.lower_limit.val_f.value_dec	= lower_limit->val_f.value_dec;
	}

	if (upper_limit != NULL) {
		/* As "upper_limit" is a union, we need just to set the "biggest" member */
		data_items[sensor_id].config.upper_limit.val_f.multiplier	= upper_limit->val_f.multiplier;
		data_items[sensor_id].config.upper_limit.val_f.value_int	= upper_limit->val_f.value_int;
		data_items[sensor_id].config.upper_limit.val_f.value_dec	= upper_limit->val_f.value_dec;
	}
	// TODO: store flags and limits on persistent storage
	return 0;
}

int knot_thing_create_schema(uint8_t i, knot_msg_schema *msg)
{
	knot_msg_schema entry;

	memset(&entry, 0, sizeof(entry));

	msg->hdr.type = KNOT_MSG_SCHEMA;

	if ((i >= KNOT_THING_DATA_MAX) || item_is_unregistered(i) == 0)
		/*
		 * FIXME
		 * Check if this is the best error to be used from the defines
		 * in the knot_protocol. Replace or create a better one if
		 * needed.
		 */
		return KNOT_SCHEMA_EMPTY;

	msg->sensor_id = i;
	entry.values.value_type = data_items[i].value_type;
	entry.values.unit = data_items[i].unit;
	entry.values.type_id = data_items[i].type_id;
	strncpy(entry.values.name, data_items[i].name,
						sizeof(entry.values.name));

	msg->hdr.payload_len = sizeof(entry.values) + sizeof(entry.sensor_id);

	memcpy(&msg->values, &entry.values, sizeof(msg->values));
	/*
	 * Every time a data item is registered we must update the max
	 * number of sensor_id so we know when schema ends;
	 */
	if (i == max_sensor_id)
		msg->hdr.type = KNOT_MSG_SCHEMA_END;

	return KNOT_SUCCESS;
}

static int data_item_read(uint8_t sensor_id, knot_msg_data *data)
{
	uint8_t len = 0, uint8_val = 0, uint8_buffer[KNOT_DATA_RAW_SIZE];
	int32_t int32_val = 0, multiplier = 0;
	uint32_t uint32_val = 0;

	if ((sensor_id >= KNOT_THING_DATA_MAX) || item_is_unregistered(sensor_id) == 0)
		return -1;

	switch (data_items[sensor_id].value_type) {
	case KNOT_VALUE_TYPE_RAW:
		if (data_items[sensor_id].functions.raw_f.read == NULL)
			return -1;
		if (data_items[sensor_id].functions.raw_f.read(uint8_buffer, &uint8_val) < 0)
			return -1;

		len = uint8_val;
		memcpy(data->payload.raw, uint8_buffer, len);
		data->hdr.payload_len = len;
		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (data_items[sensor_id].functions.bool_f.read == NULL)
			return -1;
		if (data_items[sensor_id].functions.bool_f.read(&uint8_val) < 0)
			return -1;

		len = sizeof(data->payload.values.val_b);
		data->payload.values.val_b = uint8_val;
		data->hdr.payload_len = len;
		break;
	case KNOT_VALUE_TYPE_INT:
		if (data_items[sensor_id].functions.int_f.read == NULL)
			return -1;
		if (data_items[sensor_id].functions.int_f.read(&int32_val, &multiplier) < 0)
			return -1;

		len = sizeof(data->payload.values.val_i);
		data->payload.values.val_i.value = int32_val;
		data->payload.values.val_i.multiplier = multiplier;
		data->hdr.payload_len = len;
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		if (data_items[sensor_id].functions.float_f.read == NULL)
			return -1;

		if (data_items[sensor_id].functions.float_f.read(&int32_val, &uint32_val, &multiplier) < 0)
			return -1;

		len = sizeof(data->payload.values.val_f);
		data->payload.values.val_f.value_int = int32_val;
		data->payload.values.val_f.value_dec = uint32_val;
		data->payload.values.val_f.multiplier = multiplier;
		data->hdr.payload_len = len;
		break;
	default:
		return -1;
		break;
	}

	return 0;
}

static int data_item_write(uint8_t sensor_id, knot_msg_data *data)
{
	uint8_t len;

	if ((sensor_id >= KNOT_THING_DATA_MAX) || item_is_unregistered(sensor_id) == 0)
		return -1;

	switch (data_items[sensor_id].value_type) {
	case KNOT_VALUE_TYPE_RAW:
		len = sizeof(data->payload.raw);
		if (data_items[sensor_id].functions.raw_f.write == NULL)
			return -1;
		if (data_items[sensor_id].functions.raw_f.write(data->payload.raw, &len) < 0)
			return -1;

		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (data_items[sensor_id].functions.bool_f.write == NULL)
			return -1;
		if (data_items[sensor_id].functions.bool_f.write(&data->payload.values.val_b) < 0)
			return -1;
		break;
	case KNOT_VALUE_TYPE_INT:
		if (data_items[sensor_id].functions.int_f.read == NULL)
			return -1;
		if (data_items[sensor_id].functions.int_f.write(&data->payload.values.val_i.value,
								&data->payload.values.val_i.multiplier) < 0)
			return -1;
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		if (data_items[sensor_id].functions.float_f.write == NULL)
			return -1;

		if (data_items[sensor_id].functions.float_f.write(&data->payload.values.val_f.value_int,
								&data->payload.values.val_f.value_dec,
								&data->payload.values.val_f.multiplier) < 0)
			return -1;
		break;
	default:
		return -1;
	}

	return 0;
}

int8_t knot_thing_run(void)
{
	return knot_thing_protocol_run();
}

int verify_events(knot_msg_data *data)
{
	uint8_t err = 0, comparison = 0;
	uint32_t current_time = hal_time_ms(); // update the time variable

	/*
	 * For all registered data items: verify if value
	 * changed according to the events registered.
	 */

	err = data_item_read(evt_sensor_id, data);

	if (err < 0)
		return -1;
	/* Value did not change or error: return -1, 0 means send data */
	if (data_items[evt_sensor_id].value_type == KNOT_VALUE_TYPE_RAW) {

		if (data_items[evt_sensor_id].last_value_raw == NULL)
			return -1;

		if (data->hdr.payload_len != KNOT_DATA_RAW_SIZE)
			return -1;

		if (memcmp(data_items[evt_sensor_id].last_value_raw, data->payload.raw, KNOT_DATA_RAW_SIZE) == 0)
			return -1;

		memcpy(data_items[evt_sensor_id].last_value_raw, data->payload.raw, KNOT_DATA_RAW_SIZE);
		comparison = 1;

	} else if (data_items[evt_sensor_id].value_type == KNOT_VALUE_TYPE_BOOL) {
		if (data->payload.values.val_b != data_items[evt_sensor_id].last_data.val_b) {
			comparison |= (KNOT_EVT_FLAG_CHANGE & data_items[evt_sensor_id].config.event_flags);
			data_items[evt_sensor_id].last_data.val_b = data->payload.values.val_b;
		}

	} else if (data_items[evt_sensor_id].value_type == KNOT_VALUE_TYPE_INT) {
		// TODO: add multiplier to comparison

		if (data->payload.values.val_i.value < data_items[evt_sensor_id].config.lower_limit.val_i.value)
			comparison |= (KNOT_EVT_FLAG_LOWER_THRESHOLD & data_items[evt_sensor_id].config.event_flags);
		else if (data->payload.values.val_i.value > data_items[evt_sensor_id].config.upper_limit.val_i.value)
			comparison |= (KNOT_EVT_FLAG_UPPER_THRESHOLD & data_items[evt_sensor_id].config.event_flags);
		if (data->payload.values.val_i.value != data_items[evt_sensor_id].last_data.val_i.value)
			comparison |= (KNOT_EVT_FLAG_CHANGE & data_items[evt_sensor_id].config.event_flags);

		data_items[evt_sensor_id].last_data.val_i.value = data->payload.values.val_i.value;
		data_items[evt_sensor_id].last_data.val_i.multiplier = data->payload.values.val_i.multiplier;
	} else if (data_items[evt_sensor_id].value_type == KNOT_VALUE_TYPE_FLOAT) {
		// TODO: add multiplier and decimal part to comparison
		if (data->payload.values.val_f.value_int <
						data_items[evt_sensor_id].config.lower_limit.val_f.value_int)
			comparison |= (KNOT_EVT_FLAG_LOWER_THRESHOLD & data_items[evt_sensor_id].config.event_flags);
		else if (data->payload.values.val_f.value_int >
						data_items[evt_sensor_id].config.upper_limit.val_f.value_int)
			comparison |= (KNOT_EVT_FLAG_UPPER_THRESHOLD & data_items[evt_sensor_id].config.event_flags);
		if (data->payload.values.val_f.value_int != data_items[evt_sensor_id].last_data.val_f.value_int)
			comparison |= (KNOT_EVT_FLAG_CHANGE & data_items[evt_sensor_id].config.event_flags);

		data_items[evt_sensor_id].last_data.val_f.value_int = data->payload.values.val_f.value_int;
		data_items[evt_sensor_id].last_data.val_f.value_dec = data->payload.values.val_f.value_dec;
		data_items[evt_sensor_id].last_data.val_f.multiplier = data->payload.values.val_f.multiplier;
	} else {
	// This data item is not registered with a valid value type
		return -1;
	}

	/*
	 * It is checked if the data is in time to be updated (time overflow).
	 * If yes, the last timeout value and the comparison variable are updated with the time flag.
	 */
	if ((current_time - data_items[evt_sensor_id].last_timeout) >= data_items[evt_sensor_id].config.time_sec) {
		data_items[evt_sensor_id].last_timeout = current_time;
		comparison |= (KNOT_EVT_FLAG_TIME & data_items[evt_sensor_id].config.event_flags);
	}

	/*
	 * To avoid an extensive loop we keep an variable to iterate over all
	 * sensors/actuators once at each loop. When the last sensor was verified
	 * we reinitialize the counter, otherwise we just increment it.
	 */
	evt_sensor_id++;

	if (evt_sensor_id == max_sensor_id)
		evt_sensor_id = 0;

	// Nothing changed
	if (comparison == 0)
		return -1;

	// TODO: If something changed, create message

	return 0;
}

int8_t knot_thing_init(const char *thing_name)
{
	reset_data_items();

	return knot_thing_protocol_init(thing_name, data_item_read,
				data_item_write, knot_thing_create_schema,
				knot_thing_config_data_item, verify_events);
}
