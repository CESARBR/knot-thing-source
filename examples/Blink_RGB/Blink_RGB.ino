/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <KNoTThing.h>

#define RED_LED_PIN         3
#define RED_LED_ID          1
#define RED_LED_NAME     "Red LED"

#define GREEN_LED_PIN         5
#define GREEN_LED_ID          2
#define GREEN_LED_NAME     "Green LED"

#define BLUE_LED_PIN         6
#define BLUE_LED_ID          3
#define BLUE_LED_NAME     "Blue LED"



KNoTThing thing;

static int32_t red_led = 0;
static int32_t green_led = 0;
static int32_t blue_led = 0;

static int red_read(int32_t *val, int32_t *multiplier)
{
    *val = red_led;
    Serial.print("RED LED: ");
    Serial.println(*val);
    *multiplier = 1;
    return 0;
}

static int red_write(int32_t *val, int32_t *multiplier)
{
    *val = *val%256;
    red_led = *val;

    analogWrite(RED_LED_PIN, *val);
    Serial.print("RED LED: ");
    Serial.println(*val);
      /* TODO: Save light status in EEMPROM in to handle when reboot */
    return 0;
}

static int green_read(int32_t *val, int32_t *multiplier)
{
    *val = green_led;
    Serial.print("GREEN LED: ");
    Serial.println(*val);
    *multiplier = 1;
    return 0;
}

static int green_write(int32_t *val, int32_t *multiplier)
{
    *val = *val%256;
    green_led = *val;

    analogWrite(GREEN_LED_PIN, *val);
    Serial.print("GREEN LED: ");
    Serial.println(*val);
      /* TODO: Save light status in EEMPROM in to handle when reboot */
    return 0;
}

static int blue_read(int32_t *val, int32_t *multiplier)
{
    *val = blue_led;
    Serial.print("BLUE LED: ");
    Serial.println(*val);
    *multiplier = 1;
    return 0;
}

static int blue_write(int32_t *val, int32_t *multiplier)
{
    *val = *val%256;
    green_led = *val;

    analogWrite(BLUE_LED_PIN, *val);
    Serial.print("BLUE LED: ");
    Serial.println(*val);
      /* TODO: Save light status in EEMPROM in to handle when reboot */
    return 0;
}

void setup()
{
    Serial.begin(9600);

    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(BLUE_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    /* TODO: Read lamp status from eeprom for reboot cases */
    thing.init("KNoTThing RGB");

    thing.registerIntData(RED_LED_NAME, RED_LED_ID, KNOT_TYPE_ID_LUMINOSITY,
        KNOT_UNIT_LUMINOSITY_LM, red_read, red_write);

    thing.registerIntData(GREEN_LED_NAME, GREEN_LED_ID, KNOT_TYPE_ID_LUMINOSITY,
        KNOT_UNIT_LUMINOSITY_LM, green_read, green_write);

    thing.registerIntData(BLUE_LED_NAME, BLUE_LED_ID, KNOT_TYPE_ID_LUMINOSITY,
        KNOT_UNIT_LUMINOSITY_LM, blue_read, blue_write);
    Serial.println("RGB LED KNoT Demo");

    thing.registerDefaultConfig(RED_LED_ID);

    thing.registerDefaultConfig(GREEN_LED_ID);

    thing.registerDefaultConfig(BLUE_LED_ID);

}


void loop()
{
    thing.run();
}
