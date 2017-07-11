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
#define PIN_LED_STATUS   6 //LED used to show thing status
#endif

#include <hal/avr_errno.h>
#include <hal/avr_unistd.h>
#include <hal/storage.h>
#include <hal/nrf24.h>
#include <hal/comm.h>
#include <hal/gpio.h>
#include <hal/time.h>
#include <hal/avr_log.h>
#include "knot_thing_protocol.h"
#include "knot_thing_main.h"

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

/* Intervals for LED blinking */
#define LONG_INTERVAL			2500
#define SHORT_INTERVAL			150

/* KNoT MTU */
#define MTU 256

#ifndef MIN
#define MIN(a,b)			(((a) < (b)) ? (a) : (b))
#endif

/* Retransmission timeout in ms */
#define RETRANSMISSION_TIMEOUT				20000

static knot_msg msg;
static struct nrf24_mac addr;
static unsigned long clear_time;
static uint32_t last_timeout;
static char *device_name = NULL;
static int sock = -1;
static int cli_sock = -1;
static bool schema_flag = false;
static uint8_t enable_run = 0, schema_sensor_id = 0;


/*
 * FIXME: Thing address should be received via NFC
 * Mac address must be stored in big endian format
 */
static void set_nrf24MAC(void)
{
	uint8_t mac_mask = 4;

	memset(&addr, 0, sizeof(struct nrf24_mac));
	hal_getrandom(addr.address.b + mac_mask,
					sizeof(struct nrf24_mac) - mac_mask);
	hal_storage_write_end(HAL_STORAGE_ID_MAC, &addr,
						sizeof(struct nrf24_mac));
}

int knot_thing_protocol_init(const char *thing_name)
{
	char macString[25] = {0};

	hal_gpio_pin_mode(PIN_LED_STATUS, OUTPUT);
	hal_gpio_pin_mode(CLEAR_EEPROM_PIN, INPUT_PULLUP);

	if (thing_name == NULL)
		return -1;

	device_name = (char *)thing_name;

	/* Set mac address if it's invalid on eeprom */
	hal_storage_read_end(HAL_STORAGE_ID_MAC, &addr,
						sizeof(struct nrf24_mac));
	if ((addr.address.uint64 & 0x00000000ffffffff) != 0 ||
						addr.address.uint64 == 0) {
		hal_storage_reset_end();
		set_nrf24MAC();
	}
	nrf24_mac2str(&addr, macString);

	hal_log_str("MAC ADDR");
	hal_log_str(macString);

	if (hal_comm_init("NRF0", &addr) < 0)
		return -1;

	sock = hal_comm_socket(HAL_COMM_PF_NRF24, HAL_COMM_PROTO_RAW);
	if (sock < 0)
		return -1;

	clear_time = 0;
	enable_run = 1;
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
	static uint32_t previous_status_time = 0;
	static uint16_t status_interval;
	static uint8_t nblink, led_state, previous_led_state = LOW;
	uint32_t current_status_time = hal_time_ms();

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
	uint8_t len;

	len = MIN(sizeof(msg.reg.devName), strlen(device_name));
	msg.hdr.type = KNOT_MSG_REGISTER_REQ;
	strncpy(msg.reg.devName, device_name, len);
	msg.hdr.payload_len = len;

	if (hal_comm_write(cli_sock, &(msg.buffer), sizeof(msg.hdr) + len) < 0)
		return -1;

	return 0;
}

static int read_register(void)
{
	ssize_t nbytes;

	nbytes = hal_comm_read(cli_sock, &(msg.buffer), KNOT_MSG_SIZE);

	if (nbytes > 0) {
		if (msg.cred.result != KNOT_SUCCESS)
			return -1;

		hal_storage_write_end(HAL_STORAGE_ID_UUID, msg.cred.uuid,
						KNOT_PROTOCOL_UUID_LEN);
		hal_storage_write_end(HAL_STORAGE_ID_TOKEN, msg.cred.token,
						KNOT_PROTOCOL_TOKEN_LEN);
	} else if (nbytes < 0)
		return nbytes;

	return 0;
}

static int read_auth(void)
{
	ssize_t nbytes;

	nbytes = hal_comm_read(cli_sock, &(msg.buffer), KNOT_MSG_SIZE);

	if (nbytes > 0) {
		if (msg.action.result != KNOT_SUCCESS)
			return -1;
	} else if (nbytes < 0)
		return nbytes;

	return 0;
}

static int send_schema(void)
{
	int8_t err;

	err = knot_thing_create_schema(schema_sensor_id, &(msg.schema));

	if (err < 0)
		return err;

	if (hal_comm_write(cli_sock, &(msg.buffer), 
				sizeof(msg.hdr) + msg.hdr.payload_len) < 0)
		/* TODO create a better error define in the protocol */
		return KNOT_ERROR_UNKNOWN;

	return KNOT_SUCCESS;
}

static int set_config(uint8_t sensor_id)
{
	int8_t err;

	err = knot_thing_config_data_item(msg.config.sensor_id, 
					msg.config.values.event_flags,
					msg.config.values.time_sec,
					&(msg.config.values.lower_limit),
					&(msg.config.values.upper_limit));
	if (err)
		return KNOT_ERROR_UNKNOWN;

	msg.item.sensor_id = sensor_id;
	msg.hdr.type = KNOT_MSG_CONFIG_RESP;
	msg.hdr.payload_len = sizeof(msg.item.sensor_id);

	if (hal_comm_write(cli_sock, &(msg.buffer), 
				sizeof(msg.hdr) + msg.hdr.payload_len) < 0)
		return -1;

	return 0;
}

static int set_data(uint8_t sensor_id)
{
	int8_t err;

	err = knot_thing_data_item_write(sensor_id, &(msg.data));

	/*
	 * GW must be aware if the data was succesfully set, so we resend
	 * the request only changing the header type
	 */
	msg.hdr.type = KNOT_MSG_DATA_RESP;
	/* TODO: Improve error handling: Sensor not found, invalid data, etc */
	if (err < 0)
		msg.hdr.type = KNOT_ERROR_UNKNOWN;

	if (hal_comm_write(cli_sock, &(msg.buffer), 
				sizeof(msg.hdr) + msg.hdr.payload_len) < 0)
		return -1;

	return 0;
}

static int get_data(uint8_t sensor_id)
{
	int8_t err;

	err = knot_thing_data_item_read(sensor_id, &(msg.data));

	msg.hdr.type = KNOT_MSG_DATA;
	if (err < 0)
		msg.hdr.type = KNOT_ERROR_UNKNOWN;

	msg.data.sensor_id = sensor_id;

	if (hal_comm_write(cli_sock, &(msg.buffer), 
				sizeof(msg.hdr) + msg.hdr.payload_len) < 0)
		return -1;

	return 0;
}

static inline int is_uuid(const char *string)
{
	return (string != NULL && string[8] == '-' &&
		string[13] == '-' && string[18] == '-' && string[23] == '-');
}

static int clear_data(void)
{
	unsigned long current_time;

	if (!hal_gpio_digital_read(CLEAR_EEPROM_PIN)) {
		if(clear_time == 0)
			clear_time = hal_time_ms();
		current_time = hal_time_ms();
		if ((current_time - clear_time) >= 5000)
			return 1;

		return 0;
	}

	clear_time = 0;
	current_time = 0;

	return 0;
}

static int8_t mgmt_read(void)
{
	uint8_t buffer[MTU];
	struct mgmt_nrf24_header *mhdr = (struct mgmt_nrf24_header *) buffer;
	ssize_t rbytes;

	rbytes = hal_comm_read(sock, buffer, sizeof(buffer));

	/* mgmt on bad state? */
	if (rbytes < 0 && rbytes != -EAGAIN)
		return -1;

	/* Nothing to read? */
	if (rbytes == -EAGAIN)
		return -1;

	/* Return/ignore if it is not an event? */
	if (!(mhdr->opcode & 0x0200))
		return -1;

	switch (mhdr->opcode) {

	case MGMT_EVT_NRF24_DISCONNECTED:
		hal_comm_close(cli_sock);
		return 0;
	}

	return -1;
}

static uint8_t knot_thing_protocol_connected(bool breset)
{
	static uint8_t	substate = STATE_SETUP,
			previous_state = STATE_DISCONNECTED;
	int8_t retval;

	if (breset)
		substate = STATE_SETUP;

	if (substate != STATE_ERROR) {
		if (mgmt_read() == 0)
			return STATE_DISCONNECTED;

		previous_state = substate;
	}

	/* Network message handling state machine */
	switch (substate) {
	case STATE_SETUP:
		/*
		 * If uuid/token were found, read the addresses and send
		 * the auth request, otherwise register request
		 */
		led_status(STATE_SETUP);
		hal_storage_read_end(HAL_STORAGE_ID_UUID, &(msg.auth.uuid),
					KNOT_PROTOCOL_UUID_LEN);
		hal_storage_read_end(HAL_STORAGE_ID_TOKEN, &(msg.auth.token),
				KNOT_PROTOCOL_TOKEN_LEN);

		if (is_uuid(msg.auth.uuid)) {
			substate = STATE_AUTHENTICATING;
			msg.hdr.type = KNOT_MSG_AUTH_REQ;
			msg.hdr.payload_len = KNOT_PROTOCOL_UUID_LEN + 
						KNOT_PROTOCOL_TOKEN_LEN;

			if (hal_comm_write(cli_sock, &(msg.buffer), 
				sizeof(msg.hdr) + msg.hdr.payload_len) < 0)
				substate = STATE_ERROR;
		} else {
			substate = STATE_REGISTERING;
			if (send_register() < 0)
				substate = STATE_ERROR;
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
			substate = STATE_ONLINE;
			/* Checks if all the schemas were sent to the GW and */
			hal_storage_read_end(HAL_STORAGE_ID_SCHEMA_FLAG,
					&schema_flag, sizeof(schema_flag));
			if (!schema_flag)
				substate = STATE_SCHEMA;
		}
		else if (retval != -EAGAIN)
			substate = STATE_ERROR;
		else if (hal_timeout(hal_time_ms(), last_timeout,
						RETRANSMISSION_TIMEOUT) > 0)
			substate = STATE_SETUP;
		break;

	case STATE_REGISTERING:
		led_status(STATE_REGISTERING);
		retval = read_register();
		if (!retval)
			substate = STATE_SCHEMA;
		else if (retval != -EAGAIN)
			substate = STATE_ERROR;
		else if (hal_timeout(hal_time_ms(), last_timeout,
						RETRANSMISSION_TIMEOUT) > 0)
			substate = STATE_SETUP;
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
			substate = STATE_SCHEMA_RESP;
			break;
		case KNOT_ERROR_UNKNOWN:
			substate = STATE_ERROR;
			break;
		case KNOT_SCHEMA_EMPTY:
		case KNOT_INVALID_DEVICE:
			substate = STATE_SCHEMA;
			schema_sensor_id++;
			break;
		default:
			substate = STATE_ERROR;
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
		if (hal_comm_read(cli_sock, &(msg.buffer), KNOT_MSG_SIZE) > 0) {
			if (msg.hdr.type != KNOT_MSG_SCHEMA_RESP &&
				msg.hdr.type != KNOT_MSG_SCHEMA_END_RESP)
				break;
			if (msg.action.result != KNOT_SUCCESS) {
				substate = STATE_ERROR;
				break;
			}
			if (msg.hdr.type != KNOT_MSG_SCHEMA_END_RESP) {
				substate = STATE_SCHEMA;
				schema_sensor_id++;
				break;
			}
			/* All the schemas were sent to GW */
			schema_flag = true;
			hal_storage_write_end(HAL_STORAGE_ID_SCHEMA_FLAG,
					&schema_flag, sizeof(schema_flag));
			substate = STATE_ONLINE;
			schema_sensor_id = 0;
		} else if (hal_timeout(hal_time_ms(), last_timeout,
						RETRANSMISSION_TIMEOUT) > 0)
			substate = STATE_SCHEMA;
		break;

	case STATE_ONLINE:
		led_status(STATE_ONLINE);

		if (hal_comm_read(cli_sock, &(msg.buffer), 
						KNOT_MSG_SIZE) > 0) {
			/* There is config or set data */
			switch (msg.hdr.type) {
			case KNOT_MSG_SET_CONFIG:
				set_config(msg.config.sensor_id);
				break;

			case KNOT_MSG_SET_DATA:
				set_data(msg.data.sensor_id);
				break;

			case KNOT_MSG_GET_DATA:
				get_data(msg.item.sensor_id);
				break;

			case KNOT_MSG_DATA_RESP:
				if (msg.action.result != KNOT_SUCCESS)
					substate = STATE_ERROR;
				break;

			default:
				/* Invalid command */
				break;
			}
		}
		/* If some event ocurred send msg_data */
		if (knot_thing_verify_events(&(msg.data)) == 0)
			if (hal_comm_write(cli_sock, &(msg.buffer),
			sizeof(msg.hdr) + msg.hdr.payload_len) < 0)
				substate = STATE_ERROR;
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
	static uint8_t	run_state = STATE_DISCONNECTED;
	struct nrf24_mac peer;

	if (enable_run == 0) {
		return -1;
	}

	/*
	 * Verifies if the button for eeprom clear is pressed for more than 5s
	 */
	if (clear_data()) {
		hal_storage_reset_end();
		hal_comm_close(cli_sock);
		run_state = STATE_DISCONNECTED;
		set_nrf24MAC();
	}

	/* Network message handling state machine */
	switch (run_state) {
	case STATE_DISCONNECTED:
		/* Internally listen starts broadcasting presence*/
		led_status(STATE_DISCONNECTED);
		if (hal_comm_listen(sock) < 0) {
			break;
		}

		run_state = STATE_CONNECTING;
		break;

	case STATE_CONNECTING:
		/*
		 * Try to accept GW connection request. EAGAIN means keep
		 * waiting, less then 0 means error and greater then 0 success
		 */
		led_status(STATE_CONNECTING);
		cli_sock = hal_comm_accept(sock, (void *) &peer);
		if (cli_sock == -EAGAIN)
			break;
		else if (cli_sock < 0) {
			run_state = STATE_DISCONNECTED;
			break;
		}
		run_state = knot_thing_protocol_connected(true);
		break;

	case STATE_CONNECTED:
		run_state = knot_thing_protocol_connected(false);
		break;

	default:
		/* TODO: log invalid state */
		/* TODO: close connection if needed */
		run_state = STATE_DISCONNECTED;
		break;
	}

	return 0;
}
