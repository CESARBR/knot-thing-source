 /*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

/* Flag to enable debug logs via Serial */
#define KNOT_DEBUG_ENABLED 0

#include <stdint.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <hal/avr_errno.h>
#include <hal/avr_unistd.h>
#include <hal/avr_log.h>
#define CLEAR_EEPROM_PIN 7
#define PIN_LED_STATUS   6 //LED used to show thing status
#endif

#include <hal/storage.h>
#include <hal/nrf24.h>
#include <hal/comm.h>
#include <hal/gpio.h>
#include <hal/time.h>
#include "knot_thing_protocol.h"
#include "knot_thing_main.h"
#include "knot_thing_config.h"

/* KNoT protocol client states */
#define STATE_DISCONNECTED		0
#define STATE_ACCEPTING			1
#define STATE_CONNECTED			2
#define STATE_AUTHENTICATING		3
#define STATE_REGISTERING		4
#define STATE_SCHEMA			5
#define STATE_SCHEMA_RESP		6
#define STATE_ONLINE			7
#define STATE_RUNNING			8
#define STATE_ERROR			9

/* Intervals for LED blinking */
#define LONG_INTERVAL			10000
#define SHORT_INTERVAL			1000

/* Number of times LED is blinking */
#define BLINK_DISCONNECTED		100
#define BLINK_STABLISHING		2
#define BLINK_ONLINE			1

/* Periods for LED blinking in HALT conditions */
#define NAME_ERROR			50
#define COMM_ERROR			100
#define AUTH_ERROR			250

/* Time that the clear eeprom button needs to be pressed */
#define BUTTON_PRESSED_TIME		5000

/* KNoT MTU */
#define MTU 256

#ifndef MIN
#define MIN(a,b)			(((a) < (b)) ? (a) : (b))
#endif

/* Retransmission timeout in ms */
#define RETRANSMISSION_TIMEOUT				20000

static knot_msg msg;
static struct nrf24_config config = { .mac = 0, .channel = 76 , .name = NULL};
static unsigned long clear_time;
static uint32_t last_timeout;
static uint32_t unreg_timeout;
static int sock = -1;
static int cli_sock = -1;
static bool schema_flag = false;
static uint8_t enable_run = 0, msg_sensor_id = 0;

/*
 * FIXME: Thing address should be received via NFC
 * Mac address must be stored in big endian format
 */

void(* reset_function) (void) = 0; //declare reset function @ address 0

static void set_nrf24MAC(void)
{
	hal_getrandom(config.mac.address.b, sizeof(struct nrf24_mac));
	hal_storage_write_end(HAL_STORAGE_ID_MAC, &config.mac,
						sizeof(struct nrf24_mac));
}

static void thing_disconnect_exit(void)
{
	/* reset EEPROM (UUID/Token) and generate new MAC addr */
	hal_storage_reset_end();
	set_nrf24MAC();

	/* close connection */
	knot_thing_protocol_exit();

	/* reset thing */
	reset_function();
}

static void verify_clear_data(void)
{
	if (!hal_gpio_digital_read(CLEAR_EEPROM_PIN)) {

		if (clear_time == 0)
			clear_time = hal_time_ms();

		if (hal_timeout(hal_time_ms(), clear_time, BUTTON_PRESSED_TIME)) {
			thing_disconnect_exit();
		}
	} else
		clear_time = 0;

}

static void halt_blinking_led(uint32_t period)
{
	while (1) {
		hal_gpio_digital_write(PIN_LED_STATUS, 0);
		hal_delay_ms(period);
		hal_gpio_digital_write(PIN_LED_STATUS, 1);
		hal_delay_ms(period);
		verify_clear_data();
	}
}

static int init_connection(void)
{
#if (KNOT_DEBUG_ENABLED == 1)
	char macString[25] = {0};
	nrf24_mac2str(&config.mac, macString);

	hal_log_str("MAC");
	hal_log_str(macString);
#endif
	if (hal_comm_init("NRF0", &config) < 0)
		halt_blinking_led(COMM_ERROR);

	sock = hal_comm_socket(HAL_COMM_PF_NRF24, HAL_COMM_PROTO_RAW);
	if (sock < 0)
		halt_blinking_led(COMM_ERROR);

	clear_time = 0;
	enable_run = 1;
	last_timeout = 0;

	return 0;
}

int knot_thing_protocol_init(const char *thing_name)
{
	hal_gpio_pin_mode(PIN_LED_STATUS, OUTPUT);
	hal_gpio_pin_mode(CLEAR_EEPROM_PIN, INPUT_PULLUP);

	if (thing_name == NULL)
		halt_blinking_led(NAME_ERROR);

	config.name = (const char *) thing_name;

	/* Set mac address if it's invalid on eeprom */
	hal_storage_read_end(HAL_STORAGE_ID_MAC, &config.mac,
						sizeof(struct nrf24_mac));
	/* MAC criteria: less significant 32-bits should not be zero */
	if (!(config.mac.address.uint64 & 0x00000000ffffffff)) {
		hal_storage_reset_end();
		set_nrf24MAC();
	}

	config.id = config.mac.address.uint64;

	return init_connection();
}

void knot_thing_protocol_exit(void)
{
	hal_comm_close(cli_sock);
	hal_comm_close(sock);
	hal_comm_deinit();
	enable_run = 0;
}

/*
 * The function receives the number of times LED shall blink.
 * For each number n, the status LED should flash 2 * n
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
	if (nblink >= (status * 2)) {
		nblink = 0;
		status_interval = LONG_INTERVAL;
		hal_gpio_digital_write(PIN_LED_STATUS, 0);
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



static int send_unregister(void)
{
	/* send KNOT_MSG_UNREGISTER_RESP message */
	msg.hdr.type = KNOT_MSG_UNREGISTER_RESP;
	msg.hdr.payload_len = 0;

	if (hal_comm_write(cli_sock, &(msg.buffer),
			   sizeof(msg.hdr) + msg.hdr.payload_len) < 0)
		return -1;

	unreg_timeout = hal_time_ms();

	return 0;
}

static int send_register(void)
{
	/*
	 * KNOT_MSG_REGISTER_REQ PDU should fit in nRF24 MTU in order
	 * to avoid frame segmentation. Re-transmission may happen
	 * frequently at noisy environments or if the remote is not ready.
	 */
	uint8_t name_len = NRF24_MTU - (sizeof(msg.reg.hdr) +
						sizeof(msg.reg.id));

	name_len = MIN(name_len, strlen(config.name));
	msg.hdr.type = KNOT_MSG_REGISTER_REQ;
	msg.reg.id = config.mac.address.uint64; /* Maps id to nRF24 MAC */
	strncpy(msg.reg.devName, config.name, name_len);
	msg.hdr.payload_len = name_len + sizeof(msg.reg.id);

	if (hal_comm_write(cli_sock, &(msg.buffer),
			   sizeof(msg.hdr) + msg.hdr.payload_len) < 0)
		return -1;

	return 0;
}

static int read_register(void)
{
	ssize_t nbytes;

	nbytes = hal_comm_read(cli_sock, &(msg.buffer), KNOT_MSG_SIZE);
	if (nbytes <= 0)
		return nbytes;

	if (msg.hdr.type == KNOT_MSG_UNREGISTER_REQ) {
		return send_unregister();
	}

	if (msg.hdr.type != KNOT_MSG_REGISTER_RESP)
		return -1;

	if (msg.cred.result != KNOT_SUCCESS)
		return -1;

	hal_storage_write_end(HAL_STORAGE_ID_UUID, msg.cred.uuid,
			      KNOT_PROTOCOL_UUID_LEN);
	hal_storage_write_end(HAL_STORAGE_ID_TOKEN, msg.cred.token,
			      KNOT_PROTOCOL_TOKEN_LEN);
	return 0;
}

static int read_auth(void)
{
	ssize_t nbytes;

	nbytes = hal_comm_read(cli_sock, &(msg.buffer), KNOT_MSG_SIZE);
	if (nbytes <= 0)
		return nbytes;

	if (msg.hdr.type == KNOT_MSG_UNREGISTER_REQ) {
		return send_unregister();
	}

	if (msg.hdr.type != KNOT_MSG_AUTH_RESP)
		return -1;

	if (msg.action.result != KNOT_SUCCESS)
		return -1;

	return 0;
}

static int send_schema(void)
{
	int8_t err;

	err = knot_thing_create_schema(msg_sensor_id, &(msg.schema));

	if (err < 0)
		return err;

	if (hal_comm_write(cli_sock, &(msg.buffer),
				sizeof(msg.hdr) + msg.hdr.payload_len) < 0)
		/* TODO create a better error define in the protocol */
		return KNOT_ERROR_UNKNOWN;

	return KNOT_SUCCESS;
}

static int msg_set_config(uint8_t sensor_id)
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

static int msg_set_data(uint8_t sensor_id)
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

static int msg_get_data(uint8_t sensor_id)
{
	int8_t err;

	err = knot_thing_data_item_read(sensor_id, &(msg.data));
	if (err == -2)
		return err;

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

static int8_t mgmt_read(void)
{
	uint8_t buffer[MTU];
	struct mgmt_nrf24_header *mhdr = (struct mgmt_nrf24_header *) buffer;
	ssize_t retval;

	retval = hal_comm_read(sock, buffer, sizeof(buffer));
	if (retval < 0)
		return retval;

	/* Return/ignore if it is not an event? */
	if (!(mhdr->opcode & 0x0200))
		return -EPROTO;

	switch (mhdr->opcode) {

	case MGMT_EVT_NRF24_DISCONNECTED:
		return -ENOTCONN;
	}

	return 0;
}

static void read_online_messages(void)
{
	if (hal_comm_read(cli_sock, &(msg.buffer), KNOT_MSG_SIZE) <= 0)
		return;

	/* There is a message to read */
	switch (msg.hdr.type) {
	case KNOT_MSG_SET_CONFIG:
		msg_set_config(msg.config.sensor_id);
		break;

	case KNOT_MSG_SET_DATA:
		msg_set_data(msg.data.sensor_id);
		break;

	case KNOT_MSG_GET_DATA:
		msg_get_data(msg.item.sensor_id);
		break;

	case KNOT_MSG_DATA_RESP:
		hal_log_str("DT RSP");
		if (msg.action.result != KNOT_SUCCESS) {
			hal_log_str("DT R ERR");
			msg_get_data(msg.item.sensor_id);
		}
		break;
	case KNOT_MSG_UNREGISTER_REQ:
		send_unregister();
		break;
	default:
		/* Invalid command, ignore */
		break;
	}
}

int knot_thing_protocol_run(void)
{
	static uint8_t	run_state = STATE_DISCONNECTED;

	struct nrf24_mac peer;
	int8_t retval;

	/*
	 * Verifies if the button for eeprom clear is pressed for more than 5s
	 */
	verify_clear_data();

	if (enable_run == 0) {
		return -1;
	}

	if (run_state >= STATE_CONNECTED) {
		if (mgmt_read() == -ENOTCONN)
			run_state = STATE_DISCONNECTED;
	}

	if (unreg_timeout && hal_timeout(hal_time_ms(), unreg_timeout, 10000) > 0)
		thing_disconnect_exit();

	/* Network message handling state machine */
	switch (run_state) {
	case STATE_DISCONNECTED:
		/* Internally listen starts broadcasting presence*/
		led_status(BLINK_DISCONNECTED);
		hal_comm_close(cli_sock);
		hal_log_str("DISC");
		if (hal_comm_listen(sock) < 0) {
			break;
		}

		run_state = STATE_ACCEPTING;
		hal_log_str("ACCT");
		break;

	case STATE_ACCEPTING:
		/*
		 * Try to accept GW connection request. EAGAIN means keep
		 * waiting, less then 0 means error and greater then 0 success
		 */
		led_status(BLINK_DISCONNECTED);
		cli_sock = hal_comm_accept(sock, (void *) &peer);
		if (cli_sock == -EAGAIN)
			break;
		else if (cli_sock < 0) {
			run_state = STATE_DISCONNECTED;
			break;
		}
		run_state = STATE_CONNECTED;
		hal_log_str("CONN");
		break;

	case STATE_CONNECTED:
		/*
		 * If uuid/token were found, read the addresses and send
		 * the auth request, otherwise register request
		 */
		led_status(BLINK_STABLISHING);
		hal_storage_read_end(HAL_STORAGE_ID_UUID, &(msg.auth.uuid),
					KNOT_PROTOCOL_UUID_LEN);
		hal_storage_read_end(HAL_STORAGE_ID_TOKEN, &(msg.auth.token),
				KNOT_PROTOCOL_TOKEN_LEN);

		if (is_uuid(msg.auth.uuid)) {
			run_state = STATE_AUTHENTICATING;
			hal_log_str("AUTH");
			msg.hdr.type = KNOT_MSG_AUTH_REQ;
			msg.hdr.payload_len = KNOT_PROTOCOL_UUID_LEN +
						KNOT_PROTOCOL_TOKEN_LEN;

			if (hal_comm_write(cli_sock, &(msg.buffer),
				sizeof(msg.hdr) + msg.hdr.payload_len) < 0)
				run_state = STATE_ERROR;
		} else {
			hal_log_str("REG");
			run_state = STATE_REGISTERING;
			if (send_register() < 0)
				run_state = STATE_ERROR;
		}
		last_timeout = hal_time_ms();
		break;

	/*
	 * Authenticating, Resgistering cases waits (without blocking)
	 * for an response of the respective requests, -EAGAIN means there was
	 * nothing to read so we ignore it, less then 0 an error and 0 success
	 */
	case STATE_AUTHENTICATING:
		led_status(BLINK_STABLISHING);
		retval = read_auth();
		if (retval == KNOT_SUCCESS) {
			run_state = STATE_ONLINE;
			hal_log_str("ONLN");
			/* Checks if all the schemas were sent to the GW and */
			hal_storage_read_end(HAL_STORAGE_ID_SCHEMA_FLAG,
					&schema_flag, sizeof(schema_flag));
			if (!schema_flag)
				run_state = STATE_SCHEMA;
		}
		else if (retval != -EAGAIN)
			halt_blinking_led(AUTH_ERROR);
		else if (hal_timeout(hal_time_ms(), last_timeout,
						RETRANSMISSION_TIMEOUT) > 0)
			run_state = STATE_CONNECTED;
		break;

	case STATE_REGISTERING:
		led_status(BLINK_STABLISHING);
		retval = read_register();
		if (!retval)
			run_state = STATE_SCHEMA;
		else if (retval != -EAGAIN)
			run_state = STATE_ERROR;
		else if (hal_timeout(hal_time_ms(), last_timeout,
						RETRANSMISSION_TIMEOUT) > 0)
			run_state = STATE_CONNECTED;
		break;

	/*
	 * STATE_SCHEMA tries to send an schema and go to STATE_SCHEMA_RESP to
	 * wait for the ack of this schema. If there is no schema for that
	 * msg_sensor_id, increments and stays in the STATE_SCHEMA. If an
	 * error occurs, goes to STATE_ERROR.
	 */
	case STATE_SCHEMA:
		led_status(BLINK_STABLISHING);
		hal_log_str("SCH");
		retval = send_schema();
		switch (retval) {
		case KNOT_SUCCESS:
			last_timeout = hal_time_ms();
			run_state = STATE_SCHEMA_RESP;
			break;
		case KNOT_ERROR_UNKNOWN:
			run_state = STATE_ERROR;
			break;
		case KNOT_SCHEMA_EMPTY:
		case KNOT_INVALID_DEVICE:
			run_state = STATE_SCHEMA;
			msg_sensor_id++;
			break;
		default:
			run_state = STATE_ERROR;
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
		led_status(BLINK_STABLISHING);
		hal_log_str("SCH_R");
		if (hal_comm_read(cli_sock, &(msg.buffer), KNOT_MSG_SIZE) > 0) {
			if (msg.hdr.type == KNOT_MSG_UNREGISTER_REQ) {
				send_unregister();
				break;
			}
			if (msg.hdr.type != KNOT_MSG_SCHEMA_RESP &&
				msg.hdr.type != KNOT_MSG_SCHEMA_END_RESP)
				break;
			if (msg.action.result != KNOT_SUCCESS) {
				run_state = STATE_SCHEMA;
				break;
			}
			if (msg.hdr.type != KNOT_MSG_SCHEMA_END_RESP) {
				run_state = STATE_SCHEMA;
				msg_sensor_id++;
				break;
			}
			/* All the schemas were sent to GW */
			schema_flag = true;
			hal_storage_write_end(HAL_STORAGE_ID_SCHEMA_FLAG,
					&schema_flag, sizeof(schema_flag));
			run_state = STATE_ONLINE;
			hal_log_str("ONLN");
			msg_sensor_id = 0;
		} else if (hal_timeout(hal_time_ms(), last_timeout,
						RETRANSMISSION_TIMEOUT) > 0)
			run_state = STATE_SCHEMA;
		break;

	case STATE_ONLINE:
		led_status(BLINK_ONLINE);
		read_online_messages();
		msg_sensor_id++;
		msg_get_data(msg_sensor_id);
		hal_log_str("DT");
		if (msg_sensor_id >= KNOT_THING_DATA_MAX) {
			msg_sensor_id = 0;
			run_state = STATE_RUNNING;
			hal_log_str("RUN");
		}
		break;
	case STATE_RUNNING:
		led_status(BLINK_ONLINE);
		read_online_messages();
		/* If some event ocurred send msg_data */
		if (knot_thing_verify_events(&(msg.data)) == 0) {
			if (hal_comm_write(cli_sock, &(msg.buffer),
			sizeof(msg.hdr) + msg.hdr.payload_len) < 0) {
				hal_log_str("DT ERR");
				hal_comm_write(cli_sock, &(msg.buffer),
					sizeof(msg.hdr) + msg.hdr.payload_len);
			} else {
				hal_log_str("DT");
			}
		}
		break;
	case STATE_ERROR:
		hal_gpio_digital_write(PIN_LED_STATUS, 1);
		hal_log_str("ERR");
		/* TODO: Review error state handling */
		run_state = STATE_DISCONNECTED;
		hal_delay_ms(1000);
		break;
	default:
		hal_log_str("INV");
		run_state = STATE_DISCONNECTED;
		break;
	}

	return 0;
}
