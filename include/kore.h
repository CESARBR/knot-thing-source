/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#ifndef __KORE_SENSOR_H__
#define __KORE_SENSOR_H__

struct sensor_integer {
	const char	*name;		/* Application defined sensor name */
	uint8_t		sensor_id;	/* Application defined id	*/
	uint16_t	type_id;	/* See KNOT_TYPE_ID_xxx		*/
	uint8_t		unit;		/* See KNOT_UNIT_xxx		*/

	/* Read data from sensor */
	int (*read)(int32_t *val, int32_t *multiplier);

	/* Reports new data to sensor */
	int (*write)(int32_t val, int32_t multiplier);
};

struct sensor_float {
	const char	*name;		/* Application defined sensor name */
	uint8_t		sensor_id;	/* Application defined id	*/
	uint16_t	type_id;	/* See KNOT_TYPE_ID_xxx		*/
	uint8_t		unit;		/* See KNOT_UNIT_xxx		*/

	/* Read data from sensor */
	int (*read)(int32_t *val, int32_t *multiplier, int32_t *valdec);

	/* Reports new data to sensor */
	int (*write)(int32_t val, int32_t multiplier, int32_t valdec);
};

struct sensor_bool {
	const char	*name;		/* Application defined sensor name */
	uint8_t		sensor_id;	/* Application defined id	*/
	uint16_t	type_id;	/* See KNOT_TYPE_ID_xxx		*/
	uint8_t		unit;		/* See KNOT_UNIT_xxx		*/

	/* Read data from sensor */
	int (*read)(int8_t *val);

	/* Reports new data to sensor */
	int (*write)(int8_t val);
};

struct sensor_raw {
	const char	*name;		/* Application defined sensor name */
	uint8_t		sensor_id;	/* Application defined id	*/
	uint16_t	type_id;	/* See KNOT_TYPE_ID_xxx		*/
	uint8_t		unit;		/* See KNOT_UNIT_xxx		*/

	/* Read data from sensor */
	int (*read)(void *buffer, int8_t *buffer_len);

	/* Reports new data to sensor */
	int (*write)(void *buffer, int8_t buffer_len);
};

/* KNOT core (kore) initialization functions and polling */
int8_t kore_init(void);
void kore_exit(void);

int8_t kore_run(void);

/*
 * Sensors registration function
 * For Arduino, these functions should be called from 'setup()'
 */
int8_t kore_sensor_register_integer(struct sensor_integer *sensor);
int8_t kore_sensor_register_float(struct sensor_float *sensor);
int8_t kore_sensor_register_bool(struct sensor_bool *sensor);
int8_t kore_sensor_register_raw(struct sensor_raw *sensor);

#endif /* __KORE_SENSOR_H__ */
