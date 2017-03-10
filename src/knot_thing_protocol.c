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

#ifdef ARDUINO
#include <Arduino.h>
#define CLEAR_EEPROM_PIN 7
#endif

#include "knot_thing_protocol.h"
#include "include/avr_errno.h"
#include "include/avr_unistd.h"
#include "include/storage.h"
#include "include/nrf24.h"
#include "include/comm.h"
#include "include/gpio.h"
#include "include/time.h"
#include "include/avr_log.h"

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
#define STATE_CONNECTED			2
#define STATE_SETUP			3
#define STATE_AUTHENTICATING		4
#define STATE_REGISTERING		5
#define STATE_SCHEMA			6
#define STATE_SCHEMA_RESP		7
#define STATE_ONLINE			8
#define STATE_ERROR			9
#define STATE_MAX			(STATE_ERROR+1)

/* Led used to show status on thing */
#define PIN_LED_STATUS			4
#define LONG_INTERVAL			2500
#define SHORT_INTERVAL			150

/* KNoT MTU */
#define MTU 256

#ifndef MIN
#define MIN(a,b)			(((a) < (b)) ? (a) : (b))
#endif

/* Retransmission timeout in ms */
#define RETRANSMISSION_TIMEOUT				20000

enum APPLICATION_LABELS {
	LABEL_ERROR,
	LABEL_SENT,
	LABEL_READ,
	LABEL_BYTES,
	LABEL_STATE,
	LABEL_MAX
};

static uint8_t enable_run = 0, schema_sensor_id = 0;
static char device_name[KNOT_PROTOCOL_DEVICE_NAME_LEN];
static schema_function schemaf;
static data_function thing_read;
static data_function thing_write;
static config_function configf;
static int sock = -1;
static events_function eventf;
static int cli_sock = -1;
static struct nrf24_mac addr;
static bool schema_flag = false;

/* Led status control variables */
static uint32_t current_status_time, previous_status_time = 0;
static uint8_t nblink, led_state, previous_led_state = LOW;
static uint16_t status_interval;

const char macStringProg[] PROGMEM = { "My MAC addr is:" };

const char errorString[] PROGMEM = { "Error" };
const char sentString[] PROGMEM = { "Sent" };
const char readString[] PROGMEM = { "Read" };
const char bytesString[] PROGMEM = { "bytes" };
const char stateString[] PROGMEM = { "State" };

const char *const string_table[] PROGMEM = {
	errorString,
	sentString,
	readString,
	bytesString,
	stateString
};

/*
 * FIXME: Thing address should be received via NFC
 * Mac address must be stored in big endian format
 */
void set_nrf24MAC()
{
	uint8_t mac_mask = 4;

	memset(&addr, 0, sizeof(struct nrf24_mac));
	hal_getrandom(addr.address.b + mac_mask,
					sizeof(struct nrf24_mac) - mac_mask);
	hal_storage_write_end(HAL_STORAGE_ID_MAC, &addr,
						sizeof(struct nrf24_mac));
}

static unsigned long time;
static uint32_t last_timeout;

int knot_thing_protocol_init(const char *thing_name, data_function read,
	data_function write, schema_function schema, config_function config,
							events_function event)
{
	uint8_t len;
	char logString[16];
	char macString[25] = {0};

	hal_gpio_pin_mode(PIN_LED_STATUS, OUTPUT);
	hal_gpio_pin_mode(CLEAR_EEPROM_PIN, INPUT_PULLUP);

	/* Set mac address if it's invalid on eeprom */
	hal_storage_read_end(HAL_STORAGE_ID_MAC, &addr,
						sizeof(struct nrf24_mac));
	if ((addr.address.uint64 & 0x00000000ffffffff) != 0 ||
						addr.address.uint64 == 0) {
		hal_storage_reset_end();
		set_nrf24MAC();
	}
	nrf24_mac2str(&addr, macString);

	strcpy_P(logString, (const char *)
			pgm_read_word(macStringProg));
	hal_log_error("%s", logString);
	hal_log_info("\t\"%s\"", macString);

	if (hal_comm_init("NRF0", NULL) < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return -1;
	}

	sock = hal_comm_socket(HAL_COMM_PF_NRF24, HAL_COMM_PROTO_RAW);
	if (sock < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return -1;
	}
	memset(device_name, 0, sizeof(device_name));

	len = MIN(strlen(thing_name), sizeof(device_name) - 1);
	strncpy(device_name, thing_name, len);
	time = 0;
	enable_run = 1;
	schemaf = schema;
	thing_read = read;
	thing_write = write;
	configf = config;
	eventf = event;
	last_timeout = 0;

	return 0;
}

void knot_thing_protocol_exit(void)
{
	hal_comm_close(sock);
	enable_run = 0;
}

/*
 * The function receives status of the status machine as a parameter
 * For each state, represented as a number n, the status LED should flash 2 * n
 * (once to light, another to turn off)
 */
static void led_status(uint8_t status)
{
	current_status_time = hal_time_ms();

	/*
	 * If the LED has lit and off twice the state,
	 * a set a long interval
	 */
	if (nblink >= status * 2) {
		nblink = 0;
		status_interval = LONG_INTERVAL;
	}

	/*
	 * Ensures that whenever the status changes,
	 * the blink starts by turning on the LED
	 **/
	if (status != previous_led_state) {
		previous_led_state = status;
		led_state = LOW;
	}

	if ((current_status_time - previous_status_time) >= status_interval) {
		previous_status_time = current_status_time;
		led_state = !led_state;
		hal_gpio_digital_write(PIN_LED_STATUS, led_state);

		nblink++;
		status_interval = SHORT_INTERVAL;
	}
}

static int send_register(void)
{
	ssize_t nbytes;
	knot_msg_register msg;
	uint8_t len;
	char logString[6], logBytesString[6];

	memset(&msg, 0, sizeof(msg));
	len = MIN(sizeof(msg.devName), strlen(device_name));

	msg.hdr.type = KNOT_MSG_REGISTER_REQ;
	strncpy(msg.devName, device_name, len);
	msg.hdr.payload_len = len;

	nbytes = hal_comm_write(cli_sock, &msg, sizeof(msg.hdr) + len);
	strcpy_P(logString,
		(const char *)pgm_read_word(&string_table[LABEL_SENT]));
	strcpy_P(logBytesString,
		(const char *)pgm_read_word(&string_table[LABEL_BYTES]));
	if (nbytes < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return -1;
	}

	return 0;
}

static int read_register(void)
{
	ssize_t nbytes;
	knot_msg_credential crdntl;
	char logString[6];

	memset(&crdntl, 0, sizeof(crdntl));

	nbytes = hal_comm_read(cli_sock, &crdntl, sizeof(crdntl));

	if (nbytes > 0) {
		if (crdntl.result != KNOT_SUCCESS) {
			strcpy_P(logString, (const char *)
				pgm_read_word(&string_table[LABEL_ERROR]));
			hal_log_error("%s", logString);
			return -1;
		}

		hal_storage_write_end(HAL_STORAGE_ID_UUID, crdntl.uuid,
						KNOT_PROTOCOL_UUID_LEN);
		hal_storage_write_end(HAL_STORAGE_ID_TOKEN, crdntl.token,
						KNOT_PROTOCOL_TOKEN_LEN);
	} else if (nbytes < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return nbytes;
	}

	return 0;
}

static int send_auth(const char *uuid, const char *token)
{
	knot_msg_authentication msg;
	ssize_t nbytes;
	char logString[6];

	memset(&msg, 0, sizeof(msg));

	msg.hdr.type = KNOT_MSG_AUTH_REQ;
	msg.hdr.payload_len = sizeof(msg.uuid) + sizeof(msg.token);

	strncpy(msg.uuid, uuid, sizeof(msg.uuid));
	strncpy(msg.token, token, sizeof(msg.token));

	nbytes = hal_comm_write(cli_sock, &msg, sizeof(msg.hdr) +
							msg.hdr.payload_len);
	if (nbytes < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return -1;
	}

	return 0;
}

static int read_auth(void)
{
	knot_msg_result resp;
	ssize_t nbytes;
	char logString[6];

	memset(&resp, 0, sizeof(resp));

	nbytes = hal_comm_read(cli_sock, &resp, sizeof(resp));

	if (nbytes > 0) {
		if (resp.result != KNOT_SUCCESS) {
			strcpy_P(logString, (const char *)
				pgm_read_word(&string_table[LABEL_ERROR]));
			hal_log_error("%s", logString);
			return -1;
		}
	} else if (nbytes < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return nbytes;
	}

	return 0;
}

static int send_schema(void)
{
	int8_t err;
	knot_msg_schema msg;
	ssize_t nbytes;
	char logString[6];

	memset(&msg, 0, sizeof(msg));
	err = schemaf(schema_sensor_id, &msg);

	if (err < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return err;
	}

	nbytes = hal_comm_write(cli_sock, &msg, sizeof(msg.hdr) +
							msg.hdr.payload_len);
	if (nbytes < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		/* TODO create a better error define in the protocol */
		return KNOT_ERROR_UNKNOWN;
	}

	return KNOT_SUCCESS;
}

static int config(knot_msg_config *config)
{
	int8_t err;
	knot_msg_item resp;
	ssize_t nbytes;
	char logString[6];

	err = configf(config->sensor_id, config->values.event_flags,
						config->values.time_sec,
						&config->values.lower_limit,
						&config->values.upper_limit);

	if (err) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return KNOT_ERROR_UNKNOWN;
	}

	memset(&resp, 0, sizeof(resp));
	resp.sensor_id = config->sensor_id;
	resp.hdr.type = KNOT_MSG_CONFIG_RESP;
	resp.hdr.payload_len = sizeof(resp.sensor_id);

	nbytes = hal_comm_write(cli_sock, &resp, sizeof(resp.hdr) +
							resp.hdr.payload_len);
	if (nbytes < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return -1;
	}

	return 0;
}

static int set_data(knot_msg_data *data)
{
	int8_t err;
	ssize_t nbytes;
	char logString[6];

	err = thing_write(data->sensor_id, data);

	/*
	 * GW must be aware if the data was succesfully set, so we resend
	 * the request only changing the header type
	 */
	data->hdr.type = KNOT_MSG_DATA_RESP;
	/* TODO: Improve error handling: Sensor not found, invalid data, etc */
	if (err < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		data->hdr.type = KNOT_ERROR_UNKNOWN;
	}

	nbytes = hal_comm_write(cli_sock, data, sizeof(data->hdr) +
							data->hdr.payload_len);
	if (nbytes < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return nbytes;
	}

	return 0;
}

static int get_data(knot_msg_item *item)
{
	int8_t err;
	knot_msg_data data_resp;
	ssize_t nbytes;
	char logString[6];

	memset(&data_resp, 0, sizeof(data_resp));
	err = thing_read(item->sensor_id, &data_resp);

	data_resp.hdr.type = KNOT_MSG_DATA;
	if (err < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		data_resp.hdr.type = KNOT_ERROR_UNKNOWN;
	}

	data_resp.sensor_id = item->sensor_id;

	nbytes = hal_comm_write(cli_sock, &data_resp, sizeof(data_resp.hdr) +
						data_resp.hdr.payload_len);
	if (nbytes < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return -1;
	}

	return 0;
}

static int data_resp(knot_msg_result *action)
{
	return action->result;
}

static int send_data(knot_msg_data *msg_data)
{
	int8_t err;
	char logString[6];

	err = hal_comm_write(cli_sock, msg_data,
			sizeof(msg_data->hdr) + msg_data->hdr.payload_len);
	if (err < 0) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return err;
	}

	return 0;
}

static inline int is_uuid(const char *string)
{
	return (strlen(string) == KNOT_PROTOCOL_UUID_LEN && string[8] == '-' &&
		string[13] == '-' && string[18] == '-' && string[23] == '-');
}

static int clear_data(void)
{
	unsigned long current_time;
	char logString[6];

	if (!hal_gpio_digital_read(CLEAR_EEPROM_PIN)) {
		if(time == 0)
			time = hal_time_ms();
		current_time = hal_time_ms();
		if ((current_time - time) >= 5000) {
			strcpy_P(logString, (const char *)
				pgm_read_word(&string_table[LABEL_ERROR]));
			hal_log_error("%s", logString);
			return 1;
		}
		return 0;
	}
	time = 0;
	current_time = 0;

	return 0;
}

static int8_t mgmt_read(void)
{
	uint8_t buffer[MTU];
	struct mgmt_nrf24_header *mhdr = (struct mgmt_nrf24_header *) buffer;
	ssize_t rbytes;
	char logString[9];

	rbytes = hal_comm_read(sock, buffer, sizeof(buffer));

	/* mgmt on bad state? */
	if (rbytes < 0 && rbytes != -EAGAIN) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return -1;
	}

	/* Nothing to read? */
	if (rbytes == -EAGAIN) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return -1;
	}

	/* Return/ignore if it is not an event? */
	if (!(mhdr->opcode & 0x0200)) {
		strcpy_P(logString, (const char *)
			pgm_read_word(&string_table[LABEL_ERROR]));
		hal_log_error("%s", logString);
		return -1;
	}

	switch (mhdr->opcode) {

	case MGMT_EVT_NRF24_DISCONNECTED:
		hal_comm_close(cli_sock);
		return 0;
	}

	strcpy_P(logString,
		(const char *)pgm_read_word(&string_table[LABEL_ERROR]));
	hal_log_error("%s", logString);
	return -1;
}

static uint8_t knot_thing_protocol_connected(bool breset)
{
	char uuid[KNOT_PROTOCOL_UUID_LEN + 1];
	char token[KNOT_PROTOCOL_TOKEN_LEN + 1];
	static uint8_t	state = STATE_SETUP,
			previous_state = STATE_DISCONNECTED;
	int8_t retval;
	ssize_t ilen;
	knot_msg kreq;
	knot_msg_data msg_data;

	memset(&msg_data, 0, sizeof(msg_data));

	if (breset)
		state = STATE_SETUP;

	if (state != STATE_ERROR) {
		if (mgmt_read() == 0)
			return STATE_DISCONNECTED;

		previous_state = state;
	}

	/* Network message handling state machine */
	switch (state) {
	case STATE_SETUP:
		/*
		 * If uuid/token were found, read the addresses and send
		 * the auth request, otherwise register request
		 */
		led_status(STATE_SETUP);
		memset(uuid, 0, sizeof(uuid));
		memset(token, 0, sizeof(token));
		hal_storage_read_end(HAL_STORAGE_ID_UUID, uuid,
					KNOT_PROTOCOL_UUID_LEN);
		hal_storage_read_end(HAL_STORAGE_ID_TOKEN, token,
				KNOT_PROTOCOL_TOKEN_LEN);

		if (is_uuid(uuid)) {
			state = STATE_AUTHENTICATING;
			if (send_auth(uuid, token) < 0)
				state = STATE_ERROR;
		} else {
			state = STATE_REGISTERING;
			if (send_register() < 0)
				state = STATE_ERROR;
		}
		last_timeout = hal_time_ms();
		break;
	/*
	 * Authenticating, Resgistering cases waits (without blocking)
	 * for an response of the respective requests, -EAGAIN means there was
	 * nothing to read so we ignore it, less then 0 an error and 0 success
	 */
	case STATE_AUTHENTICATING:
		led_status(STATE_AUTHENTICATING);
		retval = read_auth();
		if (!retval) {
			state = STATE_ONLINE;
			/* Checks if all the schemas were sent to the GW and */
			hal_storage_read_end(HAL_STORAGE_ID_SCHEMA_FLAG,
					&schema_flag, sizeof(schema_flag));
			if (!schema_flag)
				state = STATE_SCHEMA;
		}
		else if (retval != -EAGAIN)
			state = STATE_ERROR;
		else if (hal_timeout(hal_time_ms(), last_timeout,
						RETRANSMISSION_TIMEOUT) > 0)
			state = STATE_SETUP;
		break;

	case STATE_REGISTERING:
		led_status(STATE_REGISTERING);
		retval = read_register();
		if (!retval)
			state = STATE_SCHEMA;
		else if (retval != -EAGAIN)
			state = STATE_ERROR;
		else if (hal_timeout(hal_time_ms(), last_timeout,
						RETRANSMISSION_TIMEOUT) > 0)
			state = STATE_SETUP;
		break;
	/*
	 * STATE_SCHEMA tries to send an schema and go to STATE_SCHEMA_RESP to
	 * wait for the ack of this schema. If there is no schema for that
	 * schema_sensor_id, increments and stays in the STATE_SCHEMA. If an
	 * error occurs, goes to STATE_ERROR.
	 */
	case STATE_SCHEMA:
		led_status(STATE_SCHEMA);
		retval = send_schema();
		switch (retval) {
		case KNOT_SUCCESS:
			last_timeout = hal_time_ms();
			state = STATE_SCHEMA_RESP;
			break;
		case KNOT_ERROR_UNKNOWN:
			state = STATE_ERROR;
			break;
		case KNOT_SCHEMA_EMPTY:
		case KNOT_INVALID_DEVICE:
			state = STATE_SCHEMA;
			schema_sensor_id++;
			break;
		default:
			/* TODO: invalid command */
			break;
		}
		break;

	/*
	 * Receives the ack from the GW and returns to STATE_SCHEMA to send the
	 * next schema. If it was the ack for the last schema, goes to
	 * STATE_ONLINE. If it is not a KNOT_MSG_SCHEMA_RESP, ignores. If the
	 * result was not KNOT_SUCCESS, goes to STATE_ERROR.
	 */
	case STATE_SCHEMA_RESP:
		led_status(STATE_SCHEMA_RESP);
		ilen = hal_comm_read(cli_sock, &kreq, sizeof(kreq));
		if (ilen > 0) {
			if (kreq.hdr.type != KNOT_MSG_SCHEMA_RESP &&
				kreq.hdr.type != KNOT_MSG_SCHEMA_END_RESP)
				break;
			if (kreq.action.result != KNOT_SUCCESS) {
				state = STATE_ERROR;
				break;
			}
			if (kreq.hdr.type != KNOT_MSG_SCHEMA_END_RESP) {
				state = STATE_SCHEMA;
				schema_sensor_id++;
				break;
			}
			/* All the schemas were sent to GW */
			schema_flag = true;
			hal_storage_write_end(HAL_STORAGE_ID_SCHEMA_FLAG,
					&schema_flag, sizeof(schema_flag));
			state = STATE_ONLINE;
			schema_sensor_id = 0;
		} else if (hal_timeout(hal_time_ms(), last_timeout,
						RETRANSMISSION_TIMEOUT) > 0)
			state = STATE_SCHEMA;
		break;

	case STATE_ONLINE:
		led_status(STATE_ONLINE);

		ilen = hal_comm_read(cli_sock, &kreq, sizeof(kreq));

		if (ilen > 0) {
			/* There is config or set data */
			switch (kreq.hdr.type) {
			case KNOT_MSG_SET_CONFIG:
				config(&kreq.config);
				break;

			case KNOT_MSG_SET_DATA:
				set_data(&kreq.data);
				break;

			case KNOT_MSG_GET_DATA:
				get_data(&kreq.item);
				break;

			case KNOT_MSG_DATA_RESP:
				if (data_resp(&kreq.action))
					state = STATE_ERROR;
				break;

			default:
				/* Invalid command */
				break;
			}
		}
		/* If some event ocurred send msg_data */
		if (eventf(&msg_data) == 0)
			if (send_data(&msg_data) < 0)
				state = STATE_ERROR;
		break;

	case STATE_ERROR:
		//TODO: close connection if needed
		//TODO: wait 1s
		led_status(STATE_ERROR);
		switch (previous_state) {
		case STATE_CONNECTING:
			break;
		case STATE_AUTHENTICATING:
			break;
		case STATE_REGISTERING:
			break;
		case STATE_SCHEMA:
			break;
		case STATE_SCHEMA_RESP:
			break;
		case STATE_ONLINE:
			break;
		}
		previous_state = STATE_DISCONNECTED;
		return STATE_DISCONNECTED;

	default:
		//TODO: log invalid state
		//TODO: close connection if needed
		return STATE_DISCONNECTED;
	}
	return STATE_CONNECTED;
}

int knot_thing_protocol_run(void)
{
	static uint8_t	state = STATE_DISCONNECTED;

	if (enable_run == 0) {
		return -1;
	}

	/*
	 * Verifies if the button for eeprom clear is pressed for more than 5s
	 */
	if (clear_data()) {
		hal_storage_reset_end();
		hal_comm_close(cli_sock);
		state = STATE_DISCONNECTED;
		set_nrf24MAC();
	}

	/* Network message handling state machine */
	switch (state) {
	case STATE_DISCONNECTED:
		/* Internally listen starts broadcasting presence*/
		led_status(STATE_DISCONNECTED);
		if (hal_comm_listen(sock) < 0) {
			break;
		}

		state = STATE_CONNECTING;
		break;

	case STATE_CONNECTING:
		/*
		 * Try to accept GW connection request. EAGAIN means keep
		 * waiting, less then 0 means error and greater then 0 success
		 */
		led_status(STATE_CONNECTING);
		cli_sock = hal_comm_accept(sock, &(addr.address.uint64));
		if (cli_sock == -EAGAIN)
			break;
		else if (cli_sock < 0) {
			state = STATE_DISCONNECTED;
			break;
		}
		state = knot_thing_protocol_connected(true);
		break;

	case STATE_CONNECTED:
		state = knot_thing_protocol_connected(false);
		break;

	default:
		/* TODO: log invalid state */
		/* TODO: close connection if needed */
		state = STATE_DISCONNECTED;
		break;
	}

	return 0;
}
