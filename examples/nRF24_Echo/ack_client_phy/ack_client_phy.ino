 /*
 * Copyright (c) 2017, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <nrf24l01.h>
#include <nrf24l01_io.h>
#include <hal/avr_unistd.h>
#include <hal/time.h>
#include <hal/nrf24.h>
#include <nrf24l01_ll.h>
#include <phy_driver.h>
#include <phy_driver_nrf24.h>
#include <printf_serial.h>

#define MESSAGE "CLI-1 - TX TO SERVER"
#define MESSAGE_SIZE sizeof(MESSAGE)

#define PIPE            0
#define ACK             1
#define CH_BROADCAST    76
#define CH_RAW          22
#define DRIVER_NAME     "NRF0"

struct s_msg {
  uint8_t pipe,
          count,
          data[0];
} __attribute__ ((packed));

#define MSG_FULL_SIZE  sizeof(((struct nrf24_io_pack*)NULL)->payload)
#define MSG_SIZE  (sizeof(((struct s_msg*)NULL)->count) + MESSAGE_SIZE)

static int phy_fd;
struct addr_pipe addr = { PIPE, { 0x8D, 0xD9, 0xBE, 0x96, 0xDE }};

union {
  struct nrf24_io_pack  phy;
  struct s_msg          msg;
  uint8_t               io_pack[0];
} rx;
union {
  struct nrf24_io_pack  phy;
  struct s_msg          msg;
  uint8_t               io_pack[0];
} tx;

static int32_t tx_stamp, attempt;
static uint8_t count;
static int8_t rx_len;
static int incomingOpt = -1;   // for incoming serial data
static int incomingAck = ACK;

void setup()
{
  struct channel ch = { CH_RAW, ACK };

  Serial.begin(115200);
  printf_serial_init();

  phy_fd = phy_open(DRIVER_NAME);
  phy_ioctl(phy_fd, NRF24_CMD_SET_PIPE, &addr);
  phy_ioctl(phy_fd, NRF24_CMD_SET_CHANNEL, &ch);
  printf("Sending!!!\n");
  tx_stamp = 0;
  attempt = 0;
  count = 0;
}

void loop()
{
  if ((hal_time_ms() - tx_stamp) > 11) {
    memcpy(tx.msg.data, MESSAGE, MESSAGE_SIZE);
    tx.msg.count = count;
    tx.phy.pipe = addr.pipe;
    if (phy_write(phy_fd, tx.io_pack, MSG_SIZE) == MSG_SIZE) {
      printf("TX%d[%d:%ld]:(%d)%s\n",
              addr.pipe,
              MESSAGE_SIZE,
              attempt,
              count,
              MESSAGE);
      attempt = 0;
      if (incomingAck)
        ++count;
    } else
      ++attempt;
    tx_stamp = hal_time_ms();
  }

  rx.phy.pipe = NRF24_NO_PIPE;
  rx_len = phy_read(phy_fd, rx.io_pack, MSG_FULL_SIZE);
  if (rx_len != 0) {
    if (rx.phy.pipe == addr.pipe) {
      if (!incomingAck)
        ++count;
      printf("RX%d[%d]:%c(%d)%s\n",
              rx.phy.pipe,
              rx_len,
              rx.msg.count != (count-1) ? '*' : ' ',
              rx.msg.count,
              rx.msg.data);
    } else {
      printf("Invalid RX%d[%d]:'%s'\n", rx.phy.pipe, rx_len, rx.phy.payload);
    }
  }

  if (Serial.available() > 0) {
    // read the incoming byte:
    switch(incomingOpt) {
    case 'A':
    case 'a':
      incomingAck = Serial.read();
      incomingAck = incomingAck - '0';
      if (incomingAck != 0 && incomingAck != 1)
        incomingAck = ACK;
      {
        struct channel ch = { CH_RAW, (bool)incomingAck };
        phy_ioctl(phy_fd, NRF24_CMD_SET_CHANNEL, &ch);
      }
      incomingOpt = -1;
      break;
    default:
      incomingOpt = Serial.read();
    }
  }
}
