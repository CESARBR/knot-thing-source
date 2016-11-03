/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include "knot_thing_protocol.h"

/*KNoT client storage mapping */
#define KNOT_UUID_FLAG_ADDR		0
#define KNOT_UUID_FLAG_LEN		1
#define KNOT_UUID_ADDR			(KNOT_UUID_FLAG_ADDR + KNOT_UUID_FLAG_LEN)
#define KNOT_TOKEN_FLAG_ADDR		(KNOT_UUID_ADDR + KNOT_PROTOCOL_UUID_LEN)
#define KNOT_TOKEN_FLAG_LEN		1
#define KNOT_TOKEN_ADDR			(KNOT_TOKEN_FLAG_ADDR + KNOT_TOKEN_FLAG_LEN)

/* KNoT protocol client states */
#define STATE_DISCONNECTED		0
#define STATE_CONNECTING		1
#define STATE_AUTHENTICATING		2
#define STATE_REGISTERING		3
#define STATE_SCHEMA			4
#define STATE_ONLINE			5
#define STATE_ERROR			6
#define STATE_MAX			(STATE_ERROR+1)

static uint8_t enable_run = 0;

int knot_thing_protocol_init(uint8_t protocol, data_function read,
						data_function write)
{
	//TODO: open socket
	enable_run = 1;
}

void knot_thing_protocol_exit(void)
{
	//TODO: close socket if needed
	enable_run = 0;
}

int knot_thing_protocol_run(void)
{
	static uint8_t state = STATE_DISCONNECTED;
	uint8_t uuid_flag = 0, token_flag = 0;
	knot_msg msg = {0};

	if (enable_run == 0)
		return -1;

	/* Network message handling state machine */
	switch (state) {
	case STATE_DISCONNECTED:
		//TODO: call hal_comm_connect()
		state = STATE_CONNECTING;
	break;

	case STATE_CONNECTING:
		//TODO: verify connection status, if not connected,
		//	goto STATE_ERROR
		//TODO: verify UUID & TOKEN storage
		//	goto STATE_AUTHENTICATING or STATE_REGISTERING
		//	depending on that.
		state = STATE_REGISTERING;
	break;

	case STATE_AUTHENTICATING:
		//TODO: send Auth message
		//TODO: process Auth result
		state = STATE_ONLINE;
	break;

	case STATE_REGISTERING:
		//TODO: send register message
		//TODO: process register result
		//TODO: Store UUID and Token
		state = STATE_SCHEMA;
	break;

	case STATE_SCHEMA:
		//TODO: send schema messages
		//TODO: process schema results
		state = STATE_ONLINE;
	break;

	case STATE_ONLINE:
		//TODO: process incoming messages
		//TODO: send messages according to the events
	break;

	case STATE_ERROR:
		//TODO: log error
		//TODO: close connection if needed
		//TODO: wait 1s
		state = STATE_DISCONNECTED;
	break;

	default:
		//TODO: log invalid state
		//TODO: close connection if needed
		state = STATE_DISCONNECTED;
	break;
	}

	return 0;
}


