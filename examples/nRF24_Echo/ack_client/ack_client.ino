 /*
 * Copyright (c) 2017, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#include <nrf24l01.h>
#include <hal/time.h>
#include <hal/nrf24.h>
#include <nrf24l01_ll.h>
#include <printf_serial.h>
#include <nrf24l01_io.h>
#include <spi_bus.h>

#define MESSAGE "CLI-1 - TX TO SERVER"
#define MESSAGE_SIZE sizeof(MESSAGE)

#define PIPE            5
#define ACK             1
#define CH_BROADCAST    76
#define CH_RAW          22
#define DEV             "/dev/spidev0.0"

struct s_msg {
  uint8_t count;
  char msg[NRF24_MTU-1];
} __attribute__ ((packed));

#define MSG_SIZE (sizeof(((struct s_msg*)NULL)->count) + MESSAGE_SIZE)

static int32_t tx_stamp, attempt;
static uint8_t count;
static uint8_t spi_fd = io_setup(DEV);
static uint8_t pipe_addr[5] = {0x8D, 0xD9, 0xBE, 0x96, 0xDE};
static int8_t pipe, rx_len, tx_status;

union {
  char buffer[NRF24_MTU];
  struct s_msg data;
} rx;

union {
  char buffer[NRF24_MTU];
  struct s_msg data;
} tx;



void setup()
{
  Serial.begin(115200);
  printf_serial_init();
  nrf24l01_init(DEV, NRF24_PWR_0DBM);
  nrf24l01_open_pipe(spi_fd, PIPE, pipe_addr);
  nrf24l01_set_channel(spi_fd, CH_RAW, ACK);
  printf("Sending\n");
  tx_stamp = 0;
  attempt = 0;
  count = 0;
}

void loop()
{
  if ((hal_time_ms() - tx_stamp) > 11) {
    memcpy(tx.data.msg, MESSAGE, MESSAGE_SIZE);
    tx.data.count = count;
    nrf24l01_set_ptx(spi_fd, PIPE);
    if (nrf24l01_ptx_data(spi_fd, tx.buffer, MSG_SIZE) == 0) {
      tx_status = nrf24l01_ptx_wait_datasent(spi_fd);
      if (tx_status == 0) {
        printf("TX%d[%d:%ld]:(%d)%s\n",
                PIPE,
                MESSAGE_SIZE,
                attempt,
                count,
                MESSAGE);
        attempt = 0;
        if (ACK)
          ++count;
      } else
        ++attempt;
    } else
        printf("** TX FIFO FULL **\n");
    nrf24l01_set_prx(spi_fd);
    tx_stamp = hal_time_ms();
  }

  pipe = nrf24l01_prx_pipe_available(spi_fd);
  if (pipe != NRF24_NO_PIPE) {
    rx_len = nrf24l01_prx_data(spi_fd, rx.buffer, NRF24_MTU);
    if (rx_len != 0) {
      if (pipe == PIPE) {
        if (!ACK)
          ++count;
        printf("RX%d[%d]:%c(%d)%s\n",
                pipe,
                rx_len,
                rx.data.count!=(count-1)?'*':' ',
                rx.data.count,
                rx.data.msg);
      } else {
        printf("Invalid RX[%d:%d]:'%s'\n", pipe, rx_len, rx.buffer);
      }
    }
  }
}
