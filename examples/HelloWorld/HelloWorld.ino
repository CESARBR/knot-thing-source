/*
 * Copyright (c) 2018, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

/*
 * This is the Hello World for the KNoT Thing library
 * This example implements a connected led lamp with an on/off switch
 *
 * To run this example, it is expected that you connect the NRF24L01 Radio
 * a LED and a switch to the Arduino according to http://knot-devel.cesar.org.br
 *
 * The default behavior for this Thing is to send data every 30 seconds
 * or when the LED value changes.
 *
 * To change its behavior on the firmware side, use the function
 * registerDefaultConfig(). See the documentation and lib examples.
 */

#include <KNoTThing.h>

/*
 * If you have a KNoTThing PCB please update the #define KNOT_BOARD_VXX_XX
 * by replacing the VXX_XX with the respective board's version.
 * To see more details please check the KNoT User's Guide
 * on the http://knot-devel.cesar.org.br website.
 */

#define KNOT_BOARD_VXX_XX

#if defined (KNOT_BOARD_V01_02)
  #define LED_PIN             2
  #define SWITCH_PIN          3
#elif defined (KNOT_BOARD_V01_03)
  #define LED_PIN             5
  #define SWITCH_PIN          2
#else
  #define LED_PIN             3
  #define SWITCH_PIN          2
#endif

#define LED_ID              1
#define LED_NAME            "LED"
#define THING_NAME          "HelloWorld KNoT"
#define PRINTING_TIME       5000

/* KNoTThing instance */
KNoTThing thing;

/* Variable that holds LED value */
bool led_value = 0;
/* Variable that holds previous Switch value */
bool prev_switch_value = 0;

/* Print a timestamp via Serial */
static void printTimestamp(void)
{
	static long current_millis = 0, print_millis = 0, hour = 0, minute = 0, sec = 0;

	if (millis()-current_millis >= 1000) {
		sec++;
		if (sec >= 60) {
			minute++;
			sec = 0;
			if (minute >= 60) {
				hour++;
				minute = 0;
			}
		}
		current_millis = millis();
	}
	if (millis()-print_millis >= PRINTING_TIME) {
		Serial.print(hour);
		Serial.print(":");
		Serial.print(minute);
		Serial.print(":");
		Serial.println(sec);
		print_millis = millis();
	}
}

/* Function used by KNoTThing for read the LED value */
static int led_read(uint8_t *val)
{
	*val = led_value;
	return 0;
}

/* Function used by KNoTThing for write the LED value */
static int led_write(uint8_t *val)
{
	led_value=*val;
	Serial.print(F("LED value: "));
	Serial.println(led_value);
	digitalWrite(LED_PIN, led_value);

	return 0;
}

void setup(void)
{
	Serial.begin(115200);

	pinMode(LED_PIN, OUTPUT);
	pinMode(SWITCH_PIN, INPUT);

	/* init KNoTThing library */
	thing.init(THING_NAME);
	/* Register the LED as a data source/sink */
	thing.registerBoolData(LED_NAME, LED_ID, KNOT_TYPE_ID_SWITCH,
			KNOT_UNIT_NOT_APPLICABLE, led_read, led_write);

	/* Send data every 30 seconds */
	thing.registerDefaultConfig(LED_ID,
			KNOT_EVT_FLAG_TIME | KNOT_EVT_FLAG_CHANGE,
			30,0, 0, 0, 0);

	/* Print thing name via Serial */
	Serial.println(F(THING_NAME));
}

void loop(void)
{
	thing.run();

	if (digitalRead(SWITCH_PIN)) {
		if (prev_switch_value == 0) {
			led_value = !led_value;
			Serial.print(F("LED value: "));
			Serial.println(led_value);
		}
		prev_switch_value = 1;
	} else {
		prev_switch_value = 0;
	}

	digitalWrite(LED_PIN, led_value);
	printTimestamp();
}
