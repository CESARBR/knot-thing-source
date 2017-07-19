/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <stdint.h>

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
		uint8_t unit, rawDataFunction read, rawDataFunction write)
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

int KNoTThing::registerDefaultConfig(uint8_t sensor_id, uint8_t event_flags,
		uint16_t time_sec, int32_t upper_int, uint32_t upper_dec,
		int32_t lower_int, uint32_t lower_dec)
{
	knot_value_types lower;
	knot_value_types upper;

	lower.val_f.value_int = lower_int;
	lower.val_f.value_dec = lower_dec;
	upper.val_f.value_int = upper_int;
	upper.val_f.value_dec = upper_dec;

	return knot_thing_config_data_item(sensor_id, event_flags, time_sec,
								&lower, &upper);
}
