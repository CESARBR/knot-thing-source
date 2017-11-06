
/*
  Copyright (c) 2017, CESAR.
  All rights reserved.

  This software may be modified and distributed under the terms
  of the BSD license. See the LICENSE file for details.

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

#define MESSAGE "SRV"
#define MESSAGE_SIZE sizeof(MESSAGE)

#define PIPE_MAX        6
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

static int phy_fd;
static int8_t rx_len, tx_len;
static int32_t tx_stamp;
static int incomingOpt = -1,   // for incoming serial data
    incomingAck = ACK,
    incomingPipe = -1;

struct addr_pipe addr[PIPE_MAX] = {
  { 0, { 0x8D, 0xD9, 0xBE, 0x96, 0xDE }},
  { 1, { 0x01, 0xBE, 0xEF, 0xDE, 0x96 }},
  { 2, { 0x02, 0xBE, 0xEF, 0xDE, 0x96 }},
  { 3, { 0x03, 0xBE, 0xEF, 0xDE, 0x96 }},
  { 4, { 0x04, 0xBE, 0xEF, 0xDE, 0x96 }},
  { 5, { 0x05, 0xBE, 0xEF, 0xDE, 0x96 }}
};

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

void setup()
{
  struct channel ch = { CH_RAW, ACK };
  int8_t pipe;

  Serial.begin(115200);
  printf_serial_init();

  phy_fd = phy_open(DRIVER_NAME);
  for(pipe=0; pipe<PIPE_MAX; ++pipe) {
    phy_ioctl(phy_fd, NRF24_CMD_SET_PIPE, &addr[pipe]);
  }
  phy_ioctl(phy_fd, NRF24_CMD_SET_CHANNEL, &ch);
  printf("Listening!!!\n");
  tx_len = 0;
}

void loop()
{
  if (tx_len != 0 && (hal_time_ms() - tx_stamp) > 5) {
    memcpy(rx.io_pack, tx.io_pack, tx_len+1);
    if (phy_write(phy_fd, rx.io_pack, tx_len) == tx_len) {
      if (incomingPipe == -1 || incomingPipe == tx.phy.pipe)
      {
        printf("TX%d[%d]:", tx.msg.pipe, tx_len);
        printf("(%d)", tx.msg.count);
        printf("%s\n", tx.msg.data);
      }
      if (incomingAck)
        tx_len = 0;
    }
    tx_stamp = hal_time_ms();
  }

  rx.phy.pipe = NRF24_NO_PIPE;
  rx_len = phy_read(phy_fd, rx.io_pack, MSG_FULL_SIZE);
  if (rx_len != 0) {
    if (incomingPipe == -1 || incomingPipe == rx.phy.pipe)
    {
      printf("RX%d[%d]:", rx.msg.pipe, rx_len);
      printf("'(%d)", rx.msg.count);
      printf("%s\n", rx.msg.data);
    }
    memcpy(rx.msg.data, MESSAGE, MESSAGE_SIZE-1);
    memcpy(tx.io_pack, rx.io_pack, rx_len+1);
    tx_len = rx_len;
    tx_stamp = hal_time_ms();
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
    case 'P':
    case 'p':
      incomingPipe = Serial.read();
      incomingPipe = incomingPipe - '0';
      if (incomingPipe < 0 || incomingPipe > (PIPE_MAX-1))
        incomingPipe = -1;
      incomingOpt = -1;
      break;
    default:
      incomingOpt = Serial.read();
    }
  }
}
