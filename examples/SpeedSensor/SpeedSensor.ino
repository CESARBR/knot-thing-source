/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

/*
 * The default behavior for a Thing is to send data every 30 seconds.
 * To change its behavior on the firmware side, use the function
 * registerDefaultConfig(). See the documentation and lib examples.
 */

#include <KNoTThing.h>

#define SPEED_SENSOR_ID     3
#define SPEED_SENSOR_NAME   "Speed Sensor"

/* TODO: Change led pin to other pins (not 8-13)*/
#define LED                 13

KNoTThing thing;

static int32_t speed_value = 0;

static int speed_read(int32_t *val)
{

    *val = speed_value++;
    Serial.print(F("speed_read(): "));
    Serial.println(*val);
    return 0;
}

static int speed_write(int32_t *val)
{
    speed_value = *val;
    Serial.print(F("speed_write(): "));
    Serial.println(*val);
    return 0;
}

void setup()
{
    Serial.begin(9600);
    pinMode(LED, OUTPUT);

    thing.init("Speed");
    thing.registerIntData(SPEED_SENSOR_NAME, SPEED_SENSOR_ID, KNOT_TYPE_ID_SPEED, KNOT_UNIT_SPEED_MS, speed_read, speed_write);

    Serial.println(F("Speed Sensor KNoT Demo"));
}

void loop()
{
    thing.run();
    digitalWrite(LED, speed_value%2); // toggle LED
}
