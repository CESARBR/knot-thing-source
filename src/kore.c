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
	return -ENOSYS;
}

int8_t kore_sensor_register_float(struct sensor_float *sensor)
{
	return -ENOSYS;
}

int8_t kore_sensor_register_bool(struct sensor_bool *sensor)
{
	return -ENOSYS;
}

int8_t kore_sensor_register_raw(struct sensor_raw *sensor)
{
	return -ENOSYS;
}
