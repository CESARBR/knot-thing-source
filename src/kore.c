/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */
#include <errno.h>
#include <stdint.h>

#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include "config.h"
#include "kore.h"

static struct sensor_integer *isensor[KORE_INTEGER_SENSORS];
static struct sensor_float *fsensor[KORE_FLOAT_SENSORS];
static struct sensor_bool *bsensor[KORE_BOOL_SENSORS];
static struct sensor_raw *rsensor[KORE_RAW_SENSORS];

static int8_t isensor_num = 0;
static int8_t fsensor_num = 0;
static int8_t bsensor_num = 0;
static int8_t rsensor_num = 0;

/* Hash used to track value changes: stores last value hash */
static int32_t ihash[KORE_INTEGER_SENSORS];
static int32_t fhash[KORE_FLOAT_SENSORS];
static int8_t bhash[KORE_BOOL_SENSORS];
static int32_t rhash[KORE_RAW_SENSORS];

static inline int32_t hash(int32_t value_int, int32_t multiplier,
							int32_t value_dec)
{
	/* TODO: implement a better function */

	return (value_int ^ multiplier ^ value_dec);
}

int8_t kore_init(void)
{
	int8_t index;

	for (index = 0; index < KORE_INTEGER_SENSORS; index++)
		isensor[index] = 0;

	for (index = 0; index < KORE_FLOAT_SENSORS; index++)
		fsensor[index] = 0;

	for (index = 0; index < KORE_BOOL_SENSORS; index++)
		bsensor[index] = 0;

	for (index = 0; index < KORE_RAW_SENSORS; index++)
		rsensor[index] = 0;

	return 0;
}

void kore_exit(void)
{

}

int8_t kore_run(void)
{
	int32_t hash_value, value_int = 0, value_dec = 0, multiplier = 0;
	int8_t index, value_bool;

	/* TODO: Monitor events from network and sensors */

	/*
	 * For all registered integer sensors: verify if value
	 * from sensor has changed based on the last value read.
	 */

	/* For integer sensors: value changed? */
	for (index = 0; index < isensor_num; index++) {

		if (!isensor[index]->read)
			continue;

		if (isensor[index]->read(&value_int, &multiplier) < 0)
			continue;

		hash_value = hash(value_int, multiplier, 0);

		if (hash_value == ihash[index])
			continue;

		ihash[index] = hash_value;
	}

	/* For float sensors: value changed? */
	for (index = 0; index < fsensor_num; index++) {

		if (!fsensor[index]->read)
			continue;

		if (fsensor[index]->read(&value_int, &multiplier,
							&value_dec) < 0)
			continue;

		hash_value = hash(value_int, multiplier, value_dec);
		if (hash_value == fhash[index])
			continue;

		fhash[index] = hash_value;
	}

	/* For boolean sensors: value changed? */
	for (index = 0; index < bsensor_num; index++) {

		if (!bsensor[index]->read)
			continue;

		if (bsensor[index]->read(&value_bool) < 0)
			continue;

		if (value_bool = bhash[index])
			continue;

		bhash[index] = value_bool;
	}

	/* For raw sensors: value changed? */
	for (index = 0; index < rsensor_num; index++) {

		if (!rsensor[index]->read)
			continue;
		/*
		 * TODO: Postpone raw data change verification. It
		 * requires a fast hash function for binary data.
		 */
	}

	return 0;
}

int8_t kore_sensor_register_integer(struct sensor_integer *sensor)
{
	 /*
	  * If KORE_INTEGER_SENSORS (0) or isensor_num reached
	  * the user defined amount: return no memory.
	  */
	if (isensor_num == KORE_INTEGER_SENSORS)
		return -ENOMEM;

	isensor[isensor_num] = sensor;
	isensor_num++;

	return 0;
}

int8_t kore_sensor_register_float(struct sensor_float *sensor)
{
	 /*
	  * If KORE_FLOAT_SENSORS (0) or fsensor_num reached
	  * the user defined amount: return no memory.
	  */
	if (fsensor_num == KORE_FLOAT_SENSORS)
		return -ENOMEM;

	fsensor[fsensor_num] = sensor;
	fsensor_num++;

	return 0;
}

int8_t kore_sensor_register_bool(struct sensor_bool *sensor)
{
	 /*
	  * If KORE_BOOL_SENSORS (0) or bsensor_num reached
	  * the user defined amount: return no memory.
	  */
	if (bsensor_num == KORE_BOOL_SENSORS)
		return -ENOMEM;

	bsensor[bsensor_num] = sensor;
	bsensor_num++;

	return 0;
}

int8_t kore_sensor_register_raw(struct sensor_raw *sensor)
{
	 /*
	  * If KORE_RAW_SENSORS (0) or rsensor_num reached
	  * the user defined amount: return no memory.
	  */
	if (rsensor_num == KORE_RAW_SENSORS)
		return -ENOMEM;

	rsensor[rsensor_num] = sensor;
	rsensor_num++;

	return 0;
}
