/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <KNoTThing.h>

#define RED_LED_PIN         2
#define RED_LED_ID          1
#define RED_LED_NAME     "Red LED"

#define BLUE_LED_PIN         3
#define BLUE_LED_ID          2
#define BLUE_LED_NAME     "Blue LED"

#define GREEN_LED_PIN         4
#define GREEN_LED_ID          3
#define GREEN_LED_NAME     "Green LED"

KNoTThing thing;

static int red_read(uint8_t *val)
{
    *val = digitalRead(RED_LED_PIN);
    Serial.print("RED LED: ");
    if (*val)
      Serial.println("ON");
    else
      Serial.println("OFF");
    return 0;
}

static int red_write(uint8_t *val)
{
    digitalWrite(RED_LED_PIN, *val);
    Serial.print("RED LED: ");
    if (*val)
      Serial.println("ON");
    else
      Serial.println("OFF");
      /* TODO: Save light status in EEMPROM in to handle when reboot */
    return 0;
}

static int green_read(uint8_t *val)
{
    *val = digitalRead(GREEN_LED_PIN);
    Serial.print("GREEN LED: ");
    if (*val)
      Serial.println("ON");
    else
      Serial.println("OFF");
    return 0;
}

static int green_write(uint8_t *val)
{
    digitalWrite(GREEN_LED_PIN, *val);
    Serial.print("GREEN LED ");
    if (*val)
      Serial.println("ON");
    else
      Serial.println("OFF");
      /* TODO: Save light status in EEMPROM in to handle when reboot */
    return 0;
}

static int blue_read(uint8_t *val)
{
    *val = digitalRead(BLUE_LED_PIN);
    Serial.print("BLUE LED: ");
    if (*val)
      Serial.println("ON");
    else
      Serial.println("OFF");
    return 0;
}

static int blue_write(uint8_t *val)
{
    digitalWrite(BLUE_LED_PIN, *val);
    Serial.print("BLUE LED ");
    if (*val)
      Serial.println("ON");
    else
      Serial.println("OFF");
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
    thing.init("KNoTThing");

    thing.registerBoolData(RED_LED_NAME, RED_LED_ID, KNOT_TYPE_ID_SWITCH,
        KNOT_UNIT_NOT_APPLICABLE, red_read, red_write);
    thing.registerBoolData(GREEN_LED_NAME, GREEN_LED_ID, KNOT_TYPE_ID_SWITCH,
        KNOT_UNIT_NOT_APPLICABLE, green_read, green_write);
    thing.registerBoolData(BLUE_LED_NAME, BLUE_LED_ID, KNOT_TYPE_ID_SWITCH,
        KNOT_UNIT_NOT_APPLICABLE, blue_read, blue_write);
    Serial.println("RGB LED KNoT Demo");
}


void loop()
{
    thing.run();
}
