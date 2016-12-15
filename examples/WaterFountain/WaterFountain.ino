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

KNoTThing thing;
HX711 scale(A3, A2);
/*
 * Constants defined by magic
 */
static float k1 = 8410743;
static float k2 = 9622553;
static float ref_w = 62.6;

static float raw_kg,kg;
static float offset;

static int scale_read(int32_t *val_int, int32_t *multiplier)
{
    int32_t tmp;
    float mes;
    float a;
    float b;

    mes = scale.get_value(5);
    a = ref_w/(k2 - k1);
    b = (-1)*ref_w*k1/(k2-k1);
    raw_kg = a*mes + b;
    kg = raw_kg - offset;

    /*
     *  MASS units are defined as type INT in the knot_protocol
     *  Converting from kg to g
     */
    *val_int = kg*1000;

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
    float mes;
    float a;
    float b;

    Serial.begin(9600);
    scale.power_up();

    thing.init("KNoTThing");
    thing.registerIntData(SCALE_NAME, SCALE_ID, KNOT_TYPE_ID_MASS,
					KNOT_UNIT_MASS_G, scale_read, scale_write);
    Serial.println("Water Fountain KNoT Demo");

    /* Tares de scale when turning on */
    mes = scale.get_value(20);
    a = ref_w/(k2 - k1);
    b = (-1)*ref_w*k1/(k2-k1);
    raw_kg = a*mes + b;
    offset = raw_kg;
}


void loop()
{
    thing.run();
}
