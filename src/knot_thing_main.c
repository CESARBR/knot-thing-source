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
	knot_value_type		last_data;
	uint8_t			*last_value_raw;
	uint8_t			raw_length;
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
		item->raw_length				= 0;
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
	knot_thing_protocol_exit();
}

int8_t knot_thing_register_raw_data_item(uint8_t id, const char *name,
	uint8_t *raw_buffer, uint8_t raw_buffer_len, uint16_t type_id,
	uint8_t value_type, uint8_t unit, knot_data_functions *func)
{
	if (raw_buffer == NULL)
		return -1;

	if (raw_buffer_len > KNOT_DATA_RAW_SIZE)
		return -1;

	if (knot_thing_register_data_item(id, name, type_id, value_type,
							unit, func) != 0)
		return -1;

	/* TODO: Find an alternative way to assign raw buffer */
	data_items[last_item].last_value_raw = raw_buffer;
	data_items[last_item].raw_length = raw_buffer_len;

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

	/* Set default config */
	item->config.event_flags			= KNOT_EVT_FLAG_TIME;
	item->config.time_sec 				= 30;
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

int knot_thing_config_data_item(uint8_t id, uint8_t evflags, uint16_t time_sec,
							knot_value_type *lower,
							knot_value_type *upper)
{
	struct _data_items *item = find_item(id);

	/*Check if config is valid*/
	if (knot_config_is_valid(evflags, time_sec, lower, upper)
								!= KNOT_SUCCESS)
		return -1;

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

	return 0;
}

int knot_thing_create_schema(uint8_t id, knot_msg_schema *msg)
{
	struct _data_items *item;

	item = find_item(id);
	if (item == NULL)
		return KNOT_INVALID_DEVICE;

	msg->hdr.type = KNOT_MSG_SCHEMA;

	if (!item)
		return KNOT_INVALID_DEVICE;

	msg->sensor_id = id;
	msg->values.value_type = item->value_type;
	msg->values.unit = item->unit;
	msg->values.type_id = item->type_id;
	strncpy(msg->values.name, item->name, sizeof(msg->values.name));

	msg->hdr.payload_len = sizeof(msg->values) + sizeof(msg->sensor_id);

	/* Send 'end' for the last item (sensor or actuator). */
	if (data_items[last_item].id == id)
		msg->hdr.type = KNOT_MSG_SCHEMA_END;

	return KNOT_SUCCESS;
}

int knot_thing_data_item_read(uint8_t id, knot_msg_data *data)
{
	struct _data_items *item;
	int len;

	item = find_item(id);
	if (!item)
		return -2;

	data->hdr.payload_len = sizeof(data->sensor_id);
	switch (item->value_type) {
	case KNOT_VALUE_TYPE_RAW:
		if (item->functions.raw_f.read == NULL)
			return -1;

		len = item->functions.raw_f.read(data->payload.raw,
						 sizeof(data->payload.raw));
		if (len < 0)
			return -1;

		if (len > item->raw_length)
			return -1;

		data->hdr.payload_len += len;
		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (item->functions.bool_f.read == NULL)
			return -1;

		if (item->functions.bool_f.read(&(data->payload.val_b)) < 0)
			return -1;

		data->hdr.payload_len += sizeof(knot_value_type_bool);
		break;
	case KNOT_VALUE_TYPE_INT:
		if (item->functions.int_f.read == NULL)
			return -1;

		if (item->functions.int_f.read(
			&(data->payload.val_i.value),
			 &(data->payload.val_i.multiplier)) < 0)
			return -1;

		data->hdr.payload_len += sizeof(knot_value_type_int);
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		if (item->functions.float_f.read == NULL)
			return -1;

		if (item->functions.float_f.read(
			&(data->payload.val_f.value_int),
			&(data->payload.val_f.value_dec),
			&(data->payload.val_f.multiplier)) < 0)
			return -1;

		data->hdr.payload_len += sizeof(knot_value_type_float);
		break;
	default:
		return -1;
	}

	return 0;
}

int knot_thing_data_item_write(uint8_t id, knot_msg_data *data)
{
	int8_t ret_val = -1;
	int8_t ilen;
	struct _data_items *item;

	item = find_item(id);
	if (!item)
		return -1;

	/* Received data length */
	ilen = data->hdr.payload_len - sizeof(data->sensor_id);
	/* Setting length to send */
	data->hdr.payload_len = sizeof(data->sensor_id);

	switch (item->value_type) {
	case KNOT_VALUE_TYPE_RAW:
		if (item->functions.raw_f.write == NULL)
			goto done;

		ret_val = item->functions.raw_f.write(data->payload.raw, ilen);
		if (ret_val < 0 || ret_val > KNOT_DATA_RAW_SIZE)
			return -1;

		data->hdr.payload_len += ret_val;
		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (item->functions.bool_f.write == NULL)
			goto done;

		ret_val = item->functions.bool_f.write(
					&data->payload.val_b);
		if (ret_val < 0)
			break;

		data->hdr.payload_len += sizeof(data->payload.val_b);
		break;
	case KNOT_VALUE_TYPE_INT:
		if (item->functions.int_f.write == NULL)
			goto done;

		ret_val = item->functions.int_f.write(
					&data->payload.val_i.value,
					&data->payload.val_i.multiplier);
		if (ret_val < 0)
			break;

		data->hdr.payload_len += sizeof(data->payload.val_i);
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		if (item->functions.float_f.write == NULL)
			goto done;

		ret_val = item->functions.float_f.write(
					&data->payload.val_f.value_int,
					&data->payload.val_f.value_dec,
					&data->payload.val_f.multiplier);
		if (ret_val < 0)
			break;

		data->hdr.payload_len += sizeof(data->payload.val_f);
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

int knot_thing_verify_events(knot_msg_data *data)
{
	struct _data_items *item;
	knot_value_type *last;
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

	data->hdr.type = KNOT_MSG_DATA;
	data->sensor_id = item->id;

	if (knot_thing_data_item_read(item->id, data) < 0)
		goto none;

	last = &(item->last_data);

	/* Value did not change or error: return -1, 0 means send data */
	switch (item->value_type) {
	case KNOT_VALUE_TYPE_RAW:

		if (item->last_value_raw == NULL)
			goto none;

		if (memcmp(item->last_value_raw, data->payload.raw,
			   item->raw_length) == 0)
			goto none;

		memcpy(item->last_value_raw, data->payload.raw,
		       item->raw_length);
		comparison = 1;
		break;
	case KNOT_VALUE_TYPE_BOOL:
		if (data->payload.val_b != last->val_b) {
			comparison |= (KNOT_EVT_FLAG_CHANGE & item->config.event_flags);
			last->val_b = data->payload.val_b;
		}
		break;
	case KNOT_VALUE_TYPE_INT:
		// TODO: add multiplier to comparison
		if (data->payload.val_i.value < item->config.lower_limit.val_i.value &&
						item->lower_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_LOWER_THRESHOLD & item->config.event_flags);
			item->upper_flag = 0;
			item->lower_flag = 1;
		} else if (data->payload.val_i.value > item->config.upper_limit.val_i.value &&
			   item->upper_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_UPPER_THRESHOLD & item->config.event_flags);
			item->upper_flag = 1;
			item->lower_flag = 0;
		} else {
			if (data->payload.val_i.value < item->config.upper_limit.val_i.value)
				item->upper_flag = 0;
			if (data->payload.val_i.value > item->config.lower_limit.val_i.value)
				item->lower_flag = 0;
		}

		if (data->payload.val_i.value != last->val_i.value)
			comparison |= (KNOT_EVT_FLAG_CHANGE & item->config.event_flags);

		last->val_i.value = data->payload.val_i.value;
		last->val_i.multiplier = data->payload.val_i.multiplier;
		break;
	case KNOT_VALUE_TYPE_FLOAT:
		// TODO: add multiplier and decimal part to comparison
		if (data->payload.val_f.value_int < item->config.lower_limit.val_f.value_int &&
				item->lower_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_LOWER_THRESHOLD & item->config.event_flags);
			item->upper_flag = 0;
			item->lower_flag = 1;
		} else if (data->payload.val_f.value_int > item->config.upper_limit.val_f.value_int &&
			   item->upper_flag == 0) {
			comparison |= (KNOT_EVT_FLAG_UPPER_THRESHOLD & item->config.event_flags);
			item->upper_flag = 1;
			item->lower_flag = 0;
		} else {
			if (data->payload.val_i.value < item->config.upper_limit.val_i.value)
				item->upper_flag = 0;
			if (data->payload.val_i.value > item->config.lower_limit.val_i.value)
				item->lower_flag = 0;
		}
		if (data->payload.val_f.value_int != last->val_f.value_int)
			comparison |= (KNOT_EVT_FLAG_CHANGE & item->config.event_flags);

		last->val_f.value_int = data->payload.val_f.value_int;
		last->val_f.value_dec = data->payload.val_f.value_dec;
		last->val_f.multiplier = data->payload.val_f.multiplier;
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
	if (hal_timeout(current_time, item->last_timeout,
			(item->config.time_sec * 1000)) > 0) {
		item->last_timeout = current_time;
		comparison |= (KNOT_EVT_FLAG_TIME & item->config.event_flags);
	}

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

	return knot_thing_protocol_init(thing_name);
}
