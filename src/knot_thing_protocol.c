/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <avr_errno.h>
#include <avr_unistd.h>

#include "knot_thing_protocol.h"
#include "storage.h"
#include "comm.h"

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

#ifndef MIN
#define MIN(a,b)			(((a) < (b)) ? (a) : (b))
#endif

static uint8_t enable_run = 0;
static char uuid[KNOT_PROTOCOL_UUID_LEN];
static char token[KNOT_PROTOCOL_TOKEN_LEN];
static char device_name[KNOT_PROTOCOL_DEVICE_NAME_LEN];

int knot_thing_protocol_init(uint8_t protocol, const char *thing_name,
					data_function read, data_function write)
{
	int len;

	memset(device_name, 0, sizeof(device_name));

	len = MIN(strlen(thing_name), sizeof(device_name) - 1);
	//TODO: open socket
	strncpy(device_name, thing_name, len);
	enable_run = 1;
}

void knot_thing_protocol_exit(void)
{
	//TODO: close socket if needed
	enable_run = 0;
}

static int send_register(void)
{
	ssize_t nbytes;
	knot_msg_register msg;
	int len;

	memset(&msg, 0, sizeof(msg));
	len = MIN(sizeof(msg.devName), strlen(device_name));

	msg.hdr.type = KNOT_MSG_REGISTER_REQ;
	strncpy(msg.devName, device_name, len);
	msg.hdr.payload_len = len;
	/* FIXME: Open socket */
	nbytes = hal_comm_write(-1, &msg, sizeof(msg.hdr) + len);
	if (nbytes < 0) {
		return -1;
	}

	return 0;
}

static int read_register(void)
{
	ssize_t nbytes;
	knot_msg_credential crdntl;
	const uint8_t buffer[] = { 0x01 };

	memset(&crdntl, 0, sizeof(crdntl));
	/* FIXME: Open socket */
	nbytes = hal_comm_read(-1, &crdntl, sizeof(crdntl));

	if (nbytes == -EAGAIN)
		return -EAGAIN;

	if (nbytes > 0) {
		if (crdntl.result != KNOT_SUCCESS)
			return -1;

		hal_storage_write(KNOT_UUID_ADDR, crdntl.uuid,
						KNOT_PROTOCOL_UUID_LEN);
		hal_storage_write(KNOT_TOKEN_ADDR, crdntl.token,
						KNOT_PROTOCOL_TOKEN_LEN);

		hal_storage_write(KNOT_UUID_FLAG_ADDR, buffer, KNOT_UUID_FLAG_LEN);
		hal_storage_write(KNOT_TOKEN_FLAG_ADDR, buffer, KNOT_TOKEN_FLAG_LEN);
	} else if (nbytes < 0)
		return nbytes;

	return 0;
}

int knot_thing_protocol_run(void)
{
	static uint8_t state = STATE_DISCONNECTED;
	uint8_t uuid_flag = 0, token_flag = 0;
	int retval = 0;

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
		/*
		 * uuid/token flags indicate whether they are
		 * stored in EEPROM or not
		 */
		hal_storage_read(KNOT_UUID_FLAG_ADDR, &uuid_flag,
						KNOT_UUID_FLAG_LEN);
		hal_storage_read(KNOT_TOKEN_FLAG_ADDR, &token_flag,
						KNOT_TOKEN_FLAG_LEN);
		/*
		 * If flag was found then we read the addresses and send
		 * the auth request, otherwise register request
		 */
		if(uuid_flag && token_flag) {
			hal_storage_read(KNOT_UUID_ADDR, uuid,
						KNOT_PROTOCOL_UUID_LEN);
			hal_storage_read(KNOT_TOKEN_ADDR, token,
					KNOT_PROTOCOL_TOKEN_LEN);

			state = STATE_AUTHENTICATING;
		} else {
			if (send_register() < 0)
				state = STATE_ERROR;
			else
				state = STATE_REGISTERING;
		}
	break;

	case STATE_AUTHENTICATING:
		//TODO: send Auth message
		//TODO: process Auth result
		state = STATE_ONLINE;
	break;

	case STATE_REGISTERING:
		retval = read_register();
		if (retval < 0)
			state = STATE_ERROR;
		else if (retval == 0)
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
