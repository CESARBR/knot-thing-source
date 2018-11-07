/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <stdint.h>
#include <stdarg.h>

#include "KNoTThing.h"

KNoTThing::KNoTThing()
{

}

KNoTThing::~KNoTThing()
{
	knot_thing_exit();
}

int KNoTThing::init(const char *thing_name)
{
	return knot_thing_init(thing_name);
}

int KNoTThing::registerIntData(const char *name, uint8_t sensor_id,
				uint16_t type_id, uint8_t unit,
				intDataFunction read, intDataFunction write)
{
	knot_data_functions func;
	func.int_f.read = read;
	func.int_f.write = write;

	return knot_thing_register_data_item(sensor_id, name, type_id,
					KNOT_VALUE_TYPE_INT, unit, &func);
}

int KNoTThing::registerFloatData(const char *name, uint8_t sensor_id,
				uint16_t type_id, uint8_t unit,
				floatDataFunction read, floatDataFunction write)
{
	knot_data_functions func;
	func.float_f.read = read;
	func.float_f.write = write;

	return knot_thing_register_data_item(sensor_id, name, type_id,
					KNOT_VALUE_TYPE_FLOAT, unit, &func);

}

int KNoTThing::registerBoolData(const char *name, uint8_t sensor_id,
				uint16_t type_id, uint8_t unit,
				boolDataFunction read, boolDataFunction write)
{
	knot_data_functions func;
	func.bool_f.read = read;
	func.bool_f.write = write;

	return knot_thing_register_data_item(sensor_id, name, type_id,
					KNOT_VALUE_TYPE_BOOL, unit, &func);

}

int KNoTThing::registerRawData(const char *name, uint8_t *raw_buffer,
		uint8_t raw_buffer_len, uint8_t sensor_id, uint16_t type_id,
		uint8_t unit, rawReadFunction read, rawWriteFunction write)
{
	knot_data_functions func;
	func.raw_f.read = read;
	func.raw_f.write = write;

	return knot_thing_register_raw_data_item(sensor_id, name, raw_buffer,
		raw_buffer_len, type_id, KNOT_VALUE_TYPE_RAW, unit, &func);
}

void KNoTThing::run()
{
	knot_thing_run();
}

int KNoTThing::registerDefaultConfig(uint8_t sensor_id, ...)
{
	va_list event_args;

	uint8_t event;
	uint8_t value_type;
	uint8_t event_flags = KNOT_EVT_FLAG_NONE;
	uint16_t time_sec = 0;
	knot_value_type lower_limit;
	knot_value_type upper_limit;

	lower_limit.val_i = 0;
	upper_limit.val_i = 0;

	value_type = knot_thing_get_value_type(sensor_id);

	if(value_type < 0)
		return -1;

	/* Read arguments and set event_flags */
	va_start(event_args, sensor_id);
	do {
		event = (uint8_t) va_arg(event_args, int);
		switch(event) {
		case KNOT_EVT_FLAG_NONE:
			break;
		case KNOT_EVT_FLAG_CHANGE:
			event_flags |= KNOT_EVT_FLAG_CHANGE;
			break;
		case KNOT_EVT_FLAG_TIME:
			time_sec = (uint16_t) va_arg(event_args, int);
			event_flags |= KNOT_EVT_FLAG_TIME;
			break;
		case KNOT_EVT_FLAG_UPPER_THRESHOLD:
			if(value_type == KNOT_VALUE_TYPE_INT)
				upper_limit.val_i = (int32_t) va_arg(event_args,
							 int);
			if(value_type == KNOT_VALUE_TYPE_FLOAT)
				upper_limit.val_f = (float) va_arg(event_args,
							 double);
			event_flags |= KNOT_EVT_FLAG_UPPER_THRESHOLD;
			break;
		case KNOT_EVT_FLAG_LOWER_THRESHOLD:
			if(value_type == KNOT_VALUE_TYPE_INT)
				lower_limit.val_i = (int32_t) va_arg(event_args,
							 int);
			if(value_type == KNOT_VALUE_TYPE_FLOAT)
				lower_limit.val_f = (float) va_arg(event_args,
							 double);
			event_flags |= KNOT_EVT_FLAG_LOWER_THRESHOLD;
			break;
		default:
			va_end(event_args);
			return -1;
		}

	} while(event);
	va_end(event_args);

	return knot_thing_config_data_item(sensor_id, event_flags, time_sec,
						&lower_limit, &upper_limit);
}
