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
 * gcc $(pkg-config --cflags --libs glib-2.0) -Isrc -Iinclude \
 * -o examples/rpi src/kore.c examples/rpi.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <glib.h>

#include "config.h"
#include "kore.h"

static GMainLoop *main_loop;

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

static int speed_read(int32_t *val, int32_t *multiplier)
{
	static int32_t cvalue = 0;

	*val = cvalue++;
	fprintf(stdout, "read(): %d\n", *val);
}

struct sensor_integer isensor = {
	.name = "Speed",
	.sensor_id = 1,
	.type_id = 0x0013,	/* KNOT_TYPE_ID_SPEED */
	.unit = 0x03,		/* KNOT_UNIT_SPEED_KMH */
	.read = speed_read,
};

static gboolean loop(gpointer user_data)
{
	kore_run();

	return TRUE;
}

int main(int argc, char *argv[])
{
	int err, timeout_id;

	/*
	 * RPi fake speed sensor. This example shows how 'kore' (KNOT
	 * core) should be used to register and implement sensor
	 * read/write callbacks.
	 */

	signal(SIGTERM, sig_term);
	signal(SIGINT, sig_term);
	signal(SIGPIPE, SIG_IGN);

	main_loop = g_main_loop_new(NULL, FALSE);

	/* Register an integer sensor: should be called from Arduino setup()  */
	kore_init();
	kore_sensor_register_integer(&isensor);

	/* Calls loop() each 1 seconds: simulates Arduino loop function */
	timeout_id = g_timeout_add_seconds(1, loop, NULL);

	g_main_loop_run(main_loop);

	g_source_remove(timeout_id);

	g_main_loop_unref(main_loop);

	return 0;
}
