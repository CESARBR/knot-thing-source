#include <RF24.h>
#include <RF24_config.h>
#include <nRF24L01.h>

/*
 * This file is part of the KNOT Project
 *
 * Copyright (c) 2015, CESAR. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the CESAR nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL CESAR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#define CE_PIN		9
#define CSN_PIN		10

#define DATAOUT		11
#define DATAIN		12
#define SPICLOCK	13

#define PIN_BUTTON		5
#define PIN_LED_GREEN		4
#define PIN_LED_BLUE		3

const uint64_t pipe = 0xE8E8F0F0E3LL;

static RF24 radio(CE_PIN, CSN_PIN);

void setup(void)
{
	pinMode(DATAOUT, OUTPUT);
	pinMode(DATAIN, INPUT);
	pinMode(SPICLOCK,OUTPUT);

	Serial.begin(9600);

	radio.begin();
	radio.setCRCLength(RF24_CRC_DISABLED);
	radio.enableDynamicPayloads();
	radio.openWritingPipe(pipe);
	radio.setPALevel(RF24_PA_MAX);
	radio.setDataRate(RF24_250KBPS);
}

void loop(void)
{
	static uint16_t counter = 0;

	counter++;
	radio.write(&counter, 2);
	radio.write(&counter, 2);
	delay(500);

}
