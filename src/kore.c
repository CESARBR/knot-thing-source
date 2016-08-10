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

#include "kore.h"

int8_t kore_init(void)
{
	return 0;
}

void kore_exit(void)
{

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
