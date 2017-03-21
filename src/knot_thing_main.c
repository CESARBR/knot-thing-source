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
#include <avr/pgmspace.h>

// TODO: normalize all returning error codes


const char KNOT_THING_EMPTY_ITEM[] PROGMEM = { "EMPTY ITEM" };

static uint8_t last_id; /* Last registered id */
static uint8_t evt_sensor_id;

static struct _data_items{
	// schema values
	uint8_t			value_type;	// KNOT_VALUE_TYPE_* (int, float, bool, raw)
	uint8_t			unit;		// KNOT_UNIT_*
	uint16_t		type_id;	// KNOT_TYPE_ID_*
	const char		*name;		// App defined data item name

	/* Control the upper lower message flow */
	uint8_t lower_flag;
	uint8_t upper_flag;

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
	struct _data_items *pdata = data_items;
	int8_t count;

	last_id = 0;
	evt_sensor_id = 0;

	for (count = 0; count < KNOT_THING_DATA_MAX; ++count, ++pdata) {
		pdata->name					= (const char *)pgm_read_word(KNOT_THING_EMPTY_ITEM);
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

		pdata->lower_flag = 0;
		pdata->upper_flag = 0;
	}
}

static int data_function_is_valid(knot_data_functions *func)
{
	if (func == NULL)
		return -1;

	if (func->int_f.read == NULL && func->int_f.write == NULL)
		return -1;

	return 0;
}

static uint8_t item_is_unregistered(uint8_t id)
{
	return (!(data_items[id].config.event_flags & KNOT_EVT_FLAG_UNREGISTERED));
}

void knot_thing_exit(void)
{

}

int8_t knot_thing_register_raw_data_item(uint8_t id, const char *name,
	uint8_t *raw_buffer, uint8_t raw_buffer_len, uint16_t type_id,
	uint8_t value_type, uint8_t unit, knot_data_functions *func)
{
	if (raw_buffer == NULL)
		return -1;

	if (raw_buffer_len != KNOT_DATA_RAW_SIZE)
		return -1;

	if (knot_thing_register_data_item(id, name, type_id, value_type,
		unit, func) != 0)
		return -1;

	data_items[id].last_value_raw	= raw_buffer;

	return 0;
}


int8_t knot_thing_register_data_item(uint8_t id, const char *name,
				uint16_t type_id, uint8_t value_type,
				uint8_t unit, knot_data_functions *func)
{

	if (id >= KNOT_THING_DATA_MAX || (item_is_unregistered(id) != 0) ||
		(knot_schema_is_valid(type_id, value_type, unit) != 0) ||
		name == NULL || (data_function_is_valid(func) != 0))
		return -1;

	data_items[id].name					= name;
	data_items[id].type_id					= type_id;
	data_items[id].unit					= unit;
	data_items[id].value_type				= value_type;
	// TODO: load flags and limits from persistent storage
	/* Remove KNOT_EVT_FLAG_UNREGISTERED flag */
	data_items[id].config.event_flags			= KNOT_EVT_FLAG_NONE;
	/* As "last_data" is a union, we need just to set the "biggest" member */
	data_items[id].last_data.val_f.multiplier		= 1;
	data_items[id].last_data.val_f.value_int			= 0;
	data_items[id].last_data.val_f.value_dec			= 0;
	/* As "lower_limit" is a union, we need just to set the "biggest" member */
	data_items[id].config.lower_limit.val_f.multiplier	= 1;
	data_items[id].config.lower_limit.val_f.value_int	= 0;
	data_items[id].config.lower_limit.val_f.value_dec	= 0;
	/* As "upper_limit" is a union, we need just to set the "biggest" member */
	data_items[id].config.upper_limit.val_f.multiplier	= 1;
	data_items[id].config.upper_limit.val_f.value_int	= 0;
	data_items[id].config.upper_limit.val_f.value_dec	= 0;
	data_items[id].last_value_raw				= NULL;
	/* As "functions" is a union, we need just to set only one of its members */
	data_items[id].functions.int_f.read			= func->int_f.read;
	data_items[id].functions.int_f.write			= func->int_f.write;

	if (id > last_id)
		last_id = id;

	return 0;
}

int knot_thing_config_data_item(uint8_t id, uint8_t evflags, uint16_t time_sec,
							knot_value_types *lower,
							knot_value_types *upper)
{
	/*FIXME: Check if config is valid */
	if ((id >= KNOT_THING_DATA_MAX) || item_is_unregistered(id) == 0)
		return -1;

	data_items[id].config.event_flags = evflags;
	data_items[id].config.time_sec = time_sec;

	/*
	 * "lower/upper limit" is a union, we need
	 * just to set the "biggest" member.
	 */

	if (lower)
		memcpy(&(data_items[id].config.lower_limit), lower,
							sizeof(*lower));

	if (upper)
		memcpy(&(data_items[id].config.upper_limit), upper,
							sizeof(*upper));

	// TODO: store flags and limits on persistent storage

	return 0;
}

int knot_thing_create_schema(uint8_t i, knot_msg_schema *msg)
{
	knot_msg_schema entry;

	memset(&entry, 0, sizeof(entry));

	msg->hdr.type = KNOT_MSG_SCHEMA;

	if ((i >= KNOT_THING_DATA_MAX) || item_is_unregistered(i) == 0)
		return KNOT_INVALID_DEVICE;

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
	if (i == last_id)
		msg->hdr.type = KNOT_MSG_SCHEMA_END;

	return KNOT_SUCCESS;
}

static int data_item_read(uint8_t id, knot_msg_data *data)
{
	uint8_t len = 0, uint8_val = 0, uint8_buffer[KNOT_DATA_RAW_SIZE];
	int32_t int32_val = 0, multiplier = 0;
	uint32_t uint32_val = 0;

	if ((id >= KNOT_THING_DATA_MAX) || item_is_unregistered(id) == 0)
		return -1;

	switch (data_items[id].value_type) {
	case KNOT_VALUE_TYPE_RAW:
		if (data_items[id].functions.raw_f.read == NULL)
			return -1;

		if (data_items[id].functions.raw_f.read(uint8_buffer, &uint8_val) < 0)
			return -1;

		len = uint8_val;
		memcpy(data->payload.raw, uint8_buffer, len);
		data->hdr.payload_len = len + sizeof(data->sensor_id);
		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (data_items[id].functions.bool_f.read == NULL)
			return -1;

		if (data_items[id].functions.bool_f.read(&uint8_val) < 0)
			return -1;

		len = sizeof(data->payload.values.val_b);
		data->payload.values.val_b = uint8_val;
		data->hdr.payload_len = len + sizeof(data->sensor_id);
		break;
	case KNOT_VALUE_TYPE_INT:
		if (data_items[id].functions.int_f.read == NULL)
			return -1;

		if (data_items[id].functions.int_f.read(&int32_val, &multiplier) < 0)
			return -1;

		len = sizeof(data->payload.values.val_i);
		data->payload.values.val_i.value = int32_val;
		data->payload.values.val_i.multiplier = multiplier;
		data->hdr.payload_len = len + sizeof(data->sensor_id);
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		if (data_items[id].functions.float_f.read == NULL)
			return -1;

		if (data_items[id].functions.float_f.read(&int32_val, &uint32_val, &multiplier) < 0)
			return -1;

		len = sizeof(data->payload.values.val_f);
		data->payload.values.val_f.value_int = int32_val;
		data->payload.values.val_f.value_dec = uint32_val;
		data->payload.values.val_f.multiplier = multiplier;
		data->hdr.payload_len = len + sizeof(data->sensor_id);
		break;
	default:
		return -1;
	}

	return 0;
}

static int data_item_write(uint8_t id, knot_msg_data *data)
{
	int8_t ret_val = -1;
	uint8_t len;

	if ((id >= KNOT_THING_DATA_MAX) || item_is_unregistered(id) == 0)
		return -1;

	switch (data_items[id].value_type) {
	case KNOT_VALUE_TYPE_RAW:
		len = sizeof(data->payload.raw);
		if (data_items[id].functions.raw_f.write == NULL)
			goto done;

		ret_val = data_items[id].functions.raw_f.write(
						data->payload.raw, &len);
		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (data_items[id].functions.bool_f.write == NULL)
			goto done;

		ret_val = data_items[id].functions.bool_f.write(
					&data->payload.values.val_b);
		break;
	case KNOT_VALUE_TYPE_INT:
		if (data_items[id].functions.int_f.write == NULL)
			goto done;

		ret_val = data_items[id].functions.int_f.write(
					&data->payload.values.val_i.value,
					&data->payload.values.val_i.multiplier);
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		if (data_items[id].functions.float_f.write == NULL)
			goto done;

		ret_val = data_items[id].functions.float_f.write(
					&data->payload.values.val_f.value_int,
					&data->payload.values.val_f.value_dec,
					&data->payload.values.val_f.multiplier);
		break;
	default:
		break;
	}

done:
	return ret_val;
}

int8_t knot_thing_run(void)
{
	return knot_thing_protocol_run();
}

static int verify_events(knot_msg_data *data)
{
	struct _data_items *pdata;
	knot_value_types *last;
	uint8_t comparison = 0;
	/* Current time in miliseconds to verify sensor timeout */
	uint32_t current_time;

	/*
	 * For all registered data items: verify if value
	 * changed according to the events registered.
	 */

	if (evt_sensor_id >= KNOT_THING_DATA_MAX) {
		evt_sensor_id = 0;
		return -1;
	} else if (item_is_unregistered(evt_sensor_id) == 0) {
		evt_sensor_id++;
		return -1;
	}

	if (data_item_read(evt_sensor_id, data) < 0) {
		evt_sensor_id++;
		return -1;
	}

	pdata = &data_items[evt_sensor_id];
	last = &(pdata->last_data);

	/* Value did not change or error: return -1, 0 means send data */
	switch (pdata->value_type) {
	case KNOT_VALUE_TYPE_RAW:

		if (pdata->last_value_raw == NULL)
			return -1;

		if (data->hdr.payload_len != KNOT_DATA_RAW_SIZE)
			return -1;

		if (memcmp(pdata->last_value_raw, data->payload.raw, KNOT_DATA_RAW_SIZE) == 0)
			return -1;

		memcpy(pdata->last_value_raw, data->payload.raw, KNOT_DATA_RAW_SIZE);
		comparison = 1;
		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (data->payload.values.val_b != last->val_b) {
			comparison |= (KNOT_EVT_FLAG_CHANGE & pdata->config.event_flags);
			last->val_b = data->payload.values.val_b;
		}
		break;
	case KNOT_VALUE_TYPE_INT:
		// TODO: add multiplier to comparison
		if (data->payload.values.val_i.value < pdata->config.lower_limit.val_i.value &&
						pdata->lower_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_LOWER_THRESHOLD & pdata->config.event_flags);
			pdata->upper_flag = 0;
			pdata->lower_flag = 1;
		} else if (data->payload.values.val_i.value > pdata->config.upper_limit.val_i.value &&
			   pdata->upper_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_UPPER_THRESHOLD & pdata->config.event_flags);
			pdata->upper_flag = 1;
			pdata->lower_flag = 0;
		} else {
			if (data->payload.values.val_i.value < pdata->config.upper_limit.val_i.value)
				pdata->upper_flag = 0;
			if (data->payload.values.val_i.value > pdata->config.lower_limit.val_i.value)
				pdata->lower_flag = 0;
		}

		if (data->payload.values.val_i.value != last->val_i.value)
			comparison |= (KNOT_EVT_FLAG_CHANGE & pdata->config.event_flags);

		last->val_i.value = data->payload.values.val_i.value;
		last->val_i.multiplier = data->payload.values.val_i.multiplier;
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		// TODO: add multiplier and decimal part to comparison
		if (data->payload.values.val_f.value_int < pdata->config.lower_limit.val_f.value_int &&
				pdata->lower_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_LOWER_THRESHOLD & pdata->config.event_flags);
			pdata->upper_flag = 0;
			pdata->lower_flag = 1;
		} else if (data->payload.values.val_f.value_int > pdata->config.upper_limit.val_f.value_int &&
			   pdata->upper_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_UPPER_THRESHOLD & pdata->config.event_flags);
			pdata->upper_flag = 1;
			pdata->lower_flag = 0;
		} else {
			if (data->payload.values.val_i.value < pdata->config.upper_limit.val_i.value)
				pdata->upper_flag = 0;
			if (data->payload.values.val_i.value > pdata->config.lower_limit.val_i.value)
				pdata->lower_flag = 0;
		}
		if (data->payload.values.val_f.value_int != last->val_f.value_int)
			comparison |= (KNOT_EVT_FLAG_CHANGE & pdata->config.event_flags);

		last->val_f.value_int = data->payload.values.val_f.value_int;
		last->val_f.value_dec = data->payload.values.val_f.value_dec;
		last->val_f.multiplier = data->payload.values.val_f.multiplier;
		break;
	default:
		// This data item is not registered with a valid value type
		evt_sensor_id++;
		return -1;
	}

	/*
	 * It is checked if the data is in time to be updated (time overflow).
	 * If yes, the last timeout value and the comparison variable are updated with the time flag.
	 */
	current_time = hal_time_ms();
	if ((current_time - pdata->last_timeout) >=
		(uint32_t) pdata->config.time_sec * 1000) {
		pdata->last_timeout = current_time;
		comparison |= (KNOT_EVT_FLAG_TIME & pdata->config.event_flags);
	}

	/*
	 * To avoid an extensive loop we keep an variable to iterate over all
	 * sensors/actuators once at each loop. When the last sensor was verified
	 * we reinitialize the counter, otherwise we just increment it.
	 */
	data->hdr.type = KNOT_MSG_DATA;
	data->sensor_id = evt_sensor_id;
	evt_sensor_id++;

	if (evt_sensor_id > last_id)
		evt_sensor_id = 0;

	// Nothing changed
	if (comparison == 0)
		return -1;

	return 0;
}

int8_t knot_thing_init(const char *thing_name)
{
	reset_data_items();

	return knot_thing_protocol_init(thing_name, data_item_read,
				data_item_write, knot_thing_create_schema,
				knot_thing_config_data_item, verify_events);
}
