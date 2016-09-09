/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

/*
 * Build instructions:
 * gcc $(pkg-config --cflags --libs glib-2.0) -Isrc -I<path to protocol>/knot-protocol-source/src \
 * -o examples/rpi src/knot_thing_main.c examples/rpi.c <path to protocol>/knot-protocol-source/src/knot_protocol.c 
 *
 * PS: Knot Thing code depends on knot_protocol, so we need to compile it also.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <glib.h>

#include "knot_thing_main.h"
#include "knot_types.h"

static GMainLoop *main_loop;
static int32_t speed_value = 0;

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

static int speed_read(int32_t *val, int32_t *multiplier)
{

	*val = speed_value++;
	*multiplier = 1;
	printf("speed_read(): %d\n", *val);
	return 0;
}

static int speed_write(int32_t *val, int32_t *multiplier)
{
	speed_value = *val;
	printf("speed_write(): %d\n", *val);
	return 0;
}

static gboolean loop(gpointer user_data)
{
	knot_thing_run();

	return TRUE;
}

#define SPEED_SENSOR_ID		3
#define SPEED_SENSOR_NAME	"Speed Sensor"

int main(int argc, char *argv[])
{
	/*
	 * RPi fake speed sensor. This example shows how KNOT Thing
	 * should be used to register and implement sensor
	 * read/write callbacks.
	 */

	int err, timeout_id;

	knot_data_functions functions;
	functions.int_f.read = speed_read;
	functions.int_f.write = NULL;

	knot_data_values lower_limit, upper_limit;
	lower_limit.value_i.value = 5;
	lower_limit.value_i.multiplier = 1;
	upper_limit.value_i.value = 10;
	upper_limit.value_i.multiplier = 1;

	signal(SIGTERM, sig_term);
	signal(SIGINT, sig_term);
	signal(SIGPIPE, SIG_IGN);

	main_loop = g_main_loop_new(NULL, FALSE);
	printf("Starting...\n");
	knot_thing_init();

	/* Register an integer sensor: should be called from Arduino setup()  */
	knot_thing_register_data_item(SPEED_SENSOR_ID, SPEED_SENSOR_NAME, KNOT_TYPE_ID_SPEED, 
	KNOT_VALUE_TYPE_INT, KNOT_UNIT_SPEED_MS, &functions);

	/* Configure the sensor triggers */
	knot_thing_config_data_item(SPEED_SENSOR_ID, (KNOT_EVT_FLAG_LOWER_THRESHOLD|KNOT_EVT_FLAG_UPPER_THRESHOLD), 
	&lower_limit, &upper_limit);

	/* Calls loop() each 1 seconds: simulates Arduino loop function */
	timeout_id = g_timeout_add_seconds(1, loop, NULL);

	g_main_loop_run(main_loop);

	g_source_remove(timeout_id);

	g_main_loop_unref(main_loop);

	knot_thing_exit();

	return 0;
}
