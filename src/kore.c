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
#include <errno.h>

#include "config.h"
#include "kore.h"

static struct sensor_integer *isensor[KORE_INTEGER_SENSORS];
static struct sensor_float *fsensor[KORE_FLOAT_SENSORS];
static struct sensor_bool *bsensor[KORE_BOOL_SENSORS];
static struct sensor_raw *rsensor[KORE_RAW_SENSORS];

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
	/* TODO: Monitor events from network and sensors */

	return 0;
}

int8_t kore_sensor_register_integer(struct sensor_integer *sensor)
{
	static int isensor_num = 0;

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
	static int fsensor_num = 0;

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
	static int bsensor_num = 0;

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
	static int rsensor_num = 0;

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
