/*
  Copyright (c) 2017, CESAR.
  All rights reserved.

  This software may be modified and distributed under the terms
  of the BSD license. See the LICENSE file for details.

*/

#include <nrf24l01.h>
#include <hal/time.h>
#include <hal/nrf24.h>
#include <nrf24l01_ll.h>
#include <printf_serial.h>
#include <nrf24l01_io.h>
#include <spi_bus.h>

#define MESSAGE "SRV"
#define MESSAGE_SIZE sizeof(MESSAGE)

#define NRF24_ADDR_WIDTHS 5
#define PIPE_MAX        6
#define ACK             1
#define CH_BROADCAST    76
#define CH_RAW          22
#define DEV             "/dev/spidev0.0"

struct s_msg {
  uint8_t count;
  char msg[0];
} __attribute__ ((packed));

static int32_t tx_stamp;
static int8_t pipe, tx_pipe, rx_len, tx_len, tx_status;
static int incomingPipe = -1;   // for incoming serial data
static uint8_t spi_fd = io_setup(DEV);

static uint8_t pipe_addr[PIPE_MAX][NRF24_ADDR_WIDTHS] = {
  {0x8D, 0xD9, 0xBE, 0x96, 0xDE},
  {1, 0xBE, 0xEF, 0xDE, 0x96},
  {2, 0xBE, 0xEF, 0xDE, 0x96},
  {3, 0xBE, 0xEF, 0xDE, 0x96},
  {4, 0xBE, 0xEF, 0xDE, 0x96},
  {5, 0xBE, 0xEF, 0xDE, 0x96}
};

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
  for(pipe=0; pipe<PIPE_MAX; ++pipe)
    nrf24l01_open_pipe(spi_fd, pipe, pipe_addr[pipe]);
  nrf24l01_set_channel(spi_fd, CH_RAW, ACK);
  nrf24l01_set_prx(spi_fd);
  printf("Listening\n");
  tx_len = 0;
}

void loop()
{
  if (tx_len != 0 && (hal_time_ms() - tx_stamp) > 5) {
    memcpy(rx.buffer, tx.buffer, tx_len);
    nrf24l01_set_ptx(spi_fd, tx_pipe);
    if (nrf24l01_ptx_data(spi_fd, rx.buffer, tx_len) == 0) {
      tx_status = nrf24l01_ptx_wait_datasent(spi_fd);
      if (tx_status == 0) {
        if (incomingPipe == -1 || incomingPipe == tx_pipe)
        {
          printf("TX%d[%d]:", tx_pipe, tx_len);
          printf("(%d)", tx.data.count);
          printf("%s\n", tx.data.msg);
        }
        if (ACK)
          tx_len = 0;
      }
    } else
        printf("** TX FIFO FULL **\n");
    nrf24l01_set_prx(spi_fd);
    tx_stamp = hal_time_ms();
  }

  pipe = nrf24l01_prx_pipe_available(spi_fd);
  if (pipe != NRF24_NO_PIPE) {
    rx_len = nrf24l01_prx_data(spi_fd, rx.buffer, NRF24_MTU);
    if (rx_len != 0) {
      if (pipe < PIPE_MAX) {
        if (incomingPipe == -1 || incomingPipe == pipe)
        {
          printf("RX%d[%d]:", pipe, rx_len);
          printf("'(%d)", rx.data.count);
          printf("%s\n", rx.data.msg);
        }
        memcpy(rx.data.msg, MESSAGE, MESSAGE_SIZE-1);
        memcpy(tx.buffer, rx.buffer, rx_len);
        tx_len = rx_len;
        tx_pipe = pipe;
        tx_stamp = hal_time_ms();
      } else {
        printf("Invalid RX[%d:%d]:'%s'\n", pipe, rx_len, rx.buffer);
      }
    }
  }

  if (Serial.available() > 0) {
          // read the incoming byte:
          incomingPipe = Serial.read();
          incomingPipe = incomingPipe - '0';
          if (incomingPipe < 0 || incomingPipe > (PIPE_MAX-1))
            incomingPipe = -1;
  }
}

