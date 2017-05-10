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

#include <hal/time.h>
#include "knot_thing_config.h"
#include "knot_types.h"
#include "knot_thing_main.h"
#include <avr/pgmspace.h>

// TODO: normalize all returning error codes


const char KNOT_THING_EMPTY_ITEM[] PROGMEM = { "EMPTY ITEM" };
static uint8_t pos_count, last_item;

static struct _data_items {
	uint8_t			id;		// KNOT_ID
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

static struct _data_items *find_item(uint8_t id)
{
	uint8_t index;
	/* Sensor ID value 0 can't be used */
	if (id == 0)
		return NULL;

	for (index = 0; index < KNOT_THING_DATA_MAX; index++) {
		if (data_items[index].id == id)
			return &data_items[index];
	}

	return NULL;
}

static void reset_data_items(void)
{
	struct _data_items *item = data_items;
	int8_t count;

	pos_count = 0;
	last_item = 0;

	for (count = 0; count < KNOT_THING_DATA_MAX; ++count, ++item) {
		item->id					= 0;
		item->name					= (const char *)pgm_read_word(KNOT_THING_EMPTY_ITEM);
		item->type_id					= KNOT_TYPE_ID_INVALID;
		item->unit					= KNOT_UNIT_NOT_APPLICABLE;
		item->value_type				= KNOT_VALUE_TYPE_INVALID;
		item->config.event_flags			= KNOT_EVT_FLAG_UNREGISTERED;
		/* As "last_data" is a union, we need just to set the "biggest" member*/
		item->last_data.val_f.multiplier		= 1;
		item->last_data.val_f.value_int		= 0;
		item->last_data.val_f.value_dec		= 0;
		/* As "lower_limit" is a union, we need just to set the "biggest" member */
		item->config.lower_limit.val_f.multiplier	= 1;
		item->config.lower_limit.val_f.value_int	= 0;
		item->config.lower_limit.val_f.value_dec	= 0;
		/* As "upper_limit" is a union, we need just to set the "biggest" member */
		item->config.upper_limit.val_f.multiplier	= 1;
		item->config.upper_limit.val_f.value_int	= 0;
		item->config.upper_limit.val_f.value_dec	= 0;
		item->last_value_raw				= NULL;
		/* As "functions" is a union, we need just to set only one of its members */
		item->functions.int_f.read			= NULL;
		item->functions.int_f.write			= NULL;

		item->lower_flag = 0;
		item->upper_flag = 0;
		/* Last timeout reset */
		item->last_timeout = 0;
		/* TODO:last_value_raw needs to be cleared/reset? */
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

	/* TODO: Find an alternative way to assign raw buffer */
	data_items[last_item].last_value_raw = raw_buffer;

	return 0;
}

/*
 * TODO: investigate if index/id or a pointer to the registered item
 * can be returned in order to access/manage the entry easier.
 */
int8_t knot_thing_register_data_item(uint8_t id, const char *name,
				uint16_t type_id, uint8_t value_type,
				uint8_t unit, knot_data_functions *func)
{
	struct _data_items *item;
	uint8_t index;

	for (index = 0, item = NULL; index < KNOT_THING_DATA_MAX; index++) {
		if (data_items[index].id == 0) {
			item = &data_items[index];
			last_item  = index;
			break;
		}
	}

	if ((!item) || (knot_schema_is_valid(type_id, value_type, unit) != 0) ||
		name == NULL || (data_function_is_valid(func) != 0))
		return -1;

	item->id					= id;
	item->name					= name;
	item->type_id					= type_id;
	item->unit					= unit;
	item->value_type				= value_type;
	// TODO: load flags and limits from persistent storage
	/* Remove KNOT_EVT_FLAG_UNREGISTERED flag */
	item->config.event_flags			= KNOT_EVT_FLAG_NONE;
	/* As "last_data" is a union, we need just to set the "biggest" member */
	item->last_data.val_f.multiplier		= 1;
	item->last_data.val_f.value_int			= 0;
	item->last_data.val_f.value_dec			= 0;
	/* As "lower_limit" is a union, we need just to set the "biggest" member */
	item->config.lower_limit.val_f.multiplier	= 1;
	item->config.lower_limit.val_f.value_int	= 0;
	item->config.lower_limit.val_f.value_dec	= 0;
	/* As "upper_limit" is a union, we need just to set the "biggest" member */
	item->config.upper_limit.val_f.multiplier	= 1;
	item->config.upper_limit.val_f.value_int	= 0;
	item->config.upper_limit.val_f.value_dec	= 0;
	item->last_value_raw				= NULL;
	/* As "functions" is a union, we need just to set only one of its members */
	item->functions.int_f.read			= func->int_f.read;
	item->functions.int_f.write			= func->int_f.write;
	/* Starting last_timeout with the current time */
	item->last_timeout 				= hal_time_ms();
	return 0;
}

int8_t knot_thing_register_config_item(uint8_t id, uint8_t event_flags,
					uint16_t time_sec, int32_t upper_int,
					uint32_t upper_dec, int32_t lower_int,
					uint32_t lower_dec)
{
	struct _data_items *item;

	/*TODO: Check if exist something in eprom and save the data there*/
	/*TODO: Check if config is valid*/
	item = find_item(id);

	if (!item)
		return -1;

	item->config.event_flags = event_flags;
	item->config.time_sec = time_sec;

	item->config.lower_limit.val_f.value_int = lower_int;
	item->config.lower_limit.val_f.value_dec = lower_dec;

	item->config.upper_limit.val_f.value_int = upper_int;
	item->config.upper_limit.val_f.value_dec = upper_dec;

	return 0;
}

int knot_thing_config_data_item(uint8_t id, uint8_t evflags, uint16_t time_sec,
							knot_value_types *lower,
							knot_value_types *upper)
{
	struct _data_items *item = find_item(id);

	/* FIXME: Check if config is valid */
	if (!item)
		return -1;

	item->config.event_flags = evflags;
	item->config.time_sec = time_sec;

	/*
	 * "lower/upper limit" is a union, we need
	 * just to set the "biggest" member.
	 */

	if (lower)
		memcpy(&(item->config.lower_limit), lower, sizeof(*lower));

	if (upper)
		memcpy(&(item->config.upper_limit), upper, sizeof(*upper));

	// TODO: store flags and limits on persistent storage

	return 0;
}

int knot_thing_create_schema(uint8_t id, knot_msg_schema *msg)
{
	knot_msg_schema entry;
	struct _data_items *item;

	item = find_item(id);
	if (item == NULL)
		return KNOT_INVALID_DEVICE;

	memset(&entry, 0, sizeof(entry));

	msg->hdr.type = KNOT_MSG_SCHEMA;

	if (!item)
		return KNOT_INVALID_DEVICE;

	msg->sensor_id = id;
	entry.values.value_type = item->value_type;
	entry.values.unit = item->unit;
	entry.values.type_id = item->type_id;
	strncpy(entry.values.name, item->name, sizeof(entry.values.name));

	msg->hdr.payload_len = sizeof(entry.values) + sizeof(entry.sensor_id);

	memcpy(&msg->values, &entry.values, sizeof(msg->values));

	/* Send 'end' for the last item (sensor or actuator). */
	if (data_items[last_item].id == id)
		msg->hdr.type = KNOT_MSG_SCHEMA_END;

	return KNOT_SUCCESS;
}

static int data_item_read(uint8_t id, knot_msg_data *data)
{
	uint8_t len, uint8_val = 0, uint8_buffer[KNOT_DATA_RAW_SIZE];
	int32_t int32_val = 0, multiplier = 0;
	uint32_t uint32_val = 0;
	struct _data_items *item;

	item = find_item(id);
	if (!item)
		return -1;

	switch (item->value_type) {
	case KNOT_VALUE_TYPE_RAW:
		if (item->functions.raw_f.read == NULL)
			return -1;

		if (item->functions.raw_f.read(uint8_buffer, &uint8_val) < 0)
			return -1;

		len = uint8_val;
		memcpy(data->payload.raw, uint8_buffer, len);
		data->hdr.payload_len = len + sizeof(data->sensor_id);
		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (item->functions.bool_f.read == NULL)
			return -1;

		if (item->functions.bool_f.read(&uint8_val) < 0)
			return -1;

		len = sizeof(data->payload.values.val_b);
		data->payload.values.val_b = uint8_val;
		data->hdr.payload_len = len + sizeof(data->sensor_id);
		break;
	case KNOT_VALUE_TYPE_INT:
		if (item->functions.int_f.read == NULL)
			return -1;

		if (item->functions.int_f.read(&int32_val, &multiplier) < 0)
			return -1;

		len = sizeof(data->payload.values.val_i);
		data->payload.values.val_i.value = int32_val;
		data->payload.values.val_i.multiplier = multiplier;
		data->hdr.payload_len = len + sizeof(data->sensor_id);
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		if (item->functions.float_f.read == NULL)
			return -1;

		if (item->functions.float_f.read(&int32_val, &uint32_val,
							&multiplier) < 0)
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
	int8_t ret_val;
	uint8_t len;
	struct _data_items *item;

	item = find_item(id);
	if (!item)
		return -1;

	switch (item->value_type) {
	case KNOT_VALUE_TYPE_RAW:
		len = sizeof(data->payload.raw);
		if (item->functions.raw_f.write == NULL)
			goto done;

		ret_val = item->functions.raw_f.write(data->payload.raw, &len);
		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (item->functions.bool_f.write == NULL)
			goto done;

		ret_val = item->functions.bool_f.write(
					&data->payload.values.val_b);
		break;
	case KNOT_VALUE_TYPE_INT:
		if (item->functions.int_f.write == NULL)
			goto done;

		ret_val = item->functions.int_f.write(
					&data->payload.values.val_i.value,
					&data->payload.values.val_i.multiplier);
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		if (item->functions.float_f.write == NULL)
			goto done;

		ret_val = item->functions.float_f.write(
					&data->payload.values.val_f.value_int,
					&data->payload.values.val_f.value_dec,
					&data->payload.values.val_f.multiplier);
		break;
	default:
		ret_val = -1;
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
	struct _data_items *item;
	knot_value_types *last;
	uint8_t comparison = 0;
	/* Current time in miliseconds to verify sensor timeout */
	uint32_t current_time;

	/*
	 * To avoid an extensive loop we keep an variable to iterate over all
	 * sensors/actuators once at each loop. When the last sensor was verified
	 * we reinitialize the counter, otherwise we just increment it.
	 */

	item = &data_items[pos_count];
	if (item->id == 0)
		goto none;

	if (data_item_read(item->id, data) < 0)
		goto none;

	last = &(item->last_data);

	/* Value did not change or error: return -1, 0 means send data */
	switch (item->value_type) {
	case KNOT_VALUE_TYPE_RAW:

		if (item->last_value_raw == NULL)
			goto none;

		if (data->hdr.payload_len != KNOT_DATA_RAW_SIZE)
			goto none;

		if (memcmp(item->last_value_raw, data->payload.raw, KNOT_DATA_RAW_SIZE) == 0)
			goto none;

		memcpy(item->last_value_raw, data->payload.raw, KNOT_DATA_RAW_SIZE);
		comparison = 1;
		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (data->payload.values.val_b != last->val_b) {
			comparison |= (KNOT_EVT_FLAG_CHANGE & item->config.event_flags);
			last->val_b = data->payload.values.val_b;
		}
		break;
	case KNOT_VALUE_TYPE_INT:
		// TODO: add multiplier to comparison
		if (data->payload.values.val_i.value < item->config.lower_limit.val_i.value &&
						item->lower_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_LOWER_THRESHOLD & item->config.event_flags);
			item->upper_flag = 0;
			item->lower_flag = 1;
		} else if (data->payload.values.val_i.value > item->config.upper_limit.val_i.value &&
			   item->upper_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_UPPER_THRESHOLD & item->config.event_flags);
			item->upper_flag = 1;
			item->lower_flag = 0;
		} else {
			if (data->payload.values.val_i.value < item->config.upper_limit.val_i.value)
				item->upper_flag = 0;
			if (data->payload.values.val_i.value > item->config.lower_limit.val_i.value)
				item->lower_flag = 0;
		}

		if (data->payload.values.val_i.value != last->val_i.value)
			comparison |= (KNOT_EVT_FLAG_CHANGE & item->config.event_flags);

		last->val_i.value = data->payload.values.val_i.value;
		last->val_i.multiplier = data->payload.values.val_i.multiplier;
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		// TODO: add multiplier and decimal part to comparison
		if (data->payload.values.val_f.value_int < item->config.lower_limit.val_f.value_int &&
				item->lower_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_LOWER_THRESHOLD & item->config.event_flags);
			item->upper_flag = 0;
			item->lower_flag = 1;
		} else if (data->payload.values.val_f.value_int > item->config.upper_limit.val_f.value_int &&
			   item->upper_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_UPPER_THRESHOLD & item->config.event_flags);
			item->upper_flag = 1;
			item->lower_flag = 0;
		} else {
			if (data->payload.values.val_i.value < item->config.upper_limit.val_i.value)
				item->upper_flag = 0;
			if (data->payload.values.val_i.value > item->config.lower_limit.val_i.value)
				item->lower_flag = 0;
		}
		if (data->payload.values.val_f.value_int != last->val_f.value_int)
			comparison |= (KNOT_EVT_FLAG_CHANGE & item->config.event_flags);

		last->val_f.value_int = data->payload.values.val_f.value_int;
		last->val_f.value_dec = data->payload.values.val_f.value_dec;
		last->val_f.multiplier = data->payload.values.val_f.multiplier;
		break;
	default:
		// This data item is not registered with a valid value type
		goto none;
	}

	/*
	 * It is checked if the data is in time to be updated (time overflow).
	 * If yes, the last timeout value and the comparison variable are updated with the time flag.
	 */
	current_time = hal_time_ms();
	if ((current_time - item->last_timeout) >=
		(uint32_t) item->config.time_sec * 1000) {
		item->last_timeout = current_time;
		comparison |= (KNOT_EVT_FLAG_TIME & item->config.event_flags);
	}

	data->hdr.type = KNOT_MSG_DATA;
	data->sensor_id = item->id;

none:
	/* Wrap or increment to the next item */
	pos_count = (pos_count + 1) > last_item ? 0 : pos_count + 1;

	/* Nothing changed */
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
