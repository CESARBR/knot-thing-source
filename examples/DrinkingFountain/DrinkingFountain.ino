/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <KNoTThing.h>
#include "HX711.h"

#define SCALE_ID            2
#define SCALE_NAME          "Scale"
#define BUTTON_PIN          3

#define TIMES_READING       20
#define TARE_BUTTON_PIN     3
#define READ_INTERVAL       1000
#define BOUNCE_RANGE        200

KNoTThing thing;
HX711 scale(A3, A2);

/*
 * Constants defined by magic
 */
static float k1 = 8410743;
static float k2 = 9622553;
static float ref_w = 62.6;

static float raw_kg,kg;
static float offset = 0;
static float mes, a, b;

int button_value_current, button_value_previous = 0;
static unsigned long currentMillis, previousMillis = 0;

static int32_t previous_value = 0;

static int32_t remove_noise(int32_t value)
{
    if(value > (previous_value + BOUNCE_RANGE) || value < (previous_value - BOUNCE_RANGE)) {
        previous_value = value;
    }

    return previous_value;
}

static float get_weight(byte times)
{
    mes = scale.get_value(times);
    a = ref_w/(k2 - k1);
    b = (-1)*ref_w*k1/(k2-k1);
    raw_kg = a*mes + b;

    return raw_kg;
}

static int scale_read(int32_t *val_int, int32_t *multiplier)
{
    button_value_current =  digitalRead(BUTTON_PIN);

    /* Tares de scale when the button is pressed */
    if ((button_value_current == HIGH) && (button_value_previous == LOW)) {
        offset = get_weight(TIMES_READING);
        Serial.print("New offset: ");
        Serial.println(offset);
    }

    button_value_previous = button_value_current;

    /*
     * Read only on interval
     */
    currentMillis = millis();
    if(currentMillis - previousMillis >= READ_INTERVAL){
        previousMillis = currentMillis;
        kg = get_weight(TIMES_READING) - offset;
    }

    /*
     *  MASS units are defined as type INT in the knot_protocol
     *  Converting from kg to g and removing noise
     */
    *val_int = remove_noise(kg*1000);

    Serial.print("Scale: ");
    Serial.println(*val_int);
    *multiplier = 1;

    return 0;
}

static int scale_write(int32_t *val_int, int32_t *multiplier)
{
    float new_offset;

    new_offset = *val_int/1000.0;
    offset = new_offset;
    Serial.print("New offset: ");
    Serial.println(offset);

    return 0;
}

void setup()
{
    Serial.begin(9600);
    scale.power_up();

    thing.init("KNoTThing");
    thing.registerIntData(SCALE_NAME, SCALE_ID, KNOT_TYPE_ID_MASS,
                    KNOT_UNIT_MASS_G, scale_read, scale_write);
    Serial.println("Water Fountain KNoT Demo");
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop()
{
    thing.run();
}
