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

#define MESSAGE "CLI-1 - TESTE TX TO SERVER"
#define MESSAGE_SIZE sizeof(MESSAGE)

#define CH_BROADCAST          76
#define DEV                   "/dev/spidev0.0"

static int32_t tx_stamp;
static char rx_buffer[NRF24_MTU], tx_buffer[NRF24_MTU];
static uint8_t rx_len;
static uint8_t spi_fd = io_setup(DEV);
static uint8_t broadcast_addr[5] = {0x8D, 0xD9, 0xBE, 0x96, 0xDE};

void setup()
{
  Serial.begin(115200);
  printf_serial_init();
  nrf24l01_init(DEV, NRF24_PWR_6DBM);
  nrf24l01_set_channel(spi_fd, 0 ,CH_BROADCAST);
  nrf24l01_open_pipe(spi_fd, broadcast_addr, 0);
  printf("Broadcasting!!!\n");
  tx_stamp = 0;
}

void loop()
{
  if ((hal_time_ms() - tx_stamp) > 5) {
    memcpy(tx_buffer, MESSAGE, MESSAGE_SIZE);
    nrf24l01_set_ptx(spi_fd, 0);
    if (nrf24l01_ptx_data(spi_fd, tx_buffer, MESSAGE_SIZE) == 0) {
      nrf24l01_ptx_wait_datasent(spi_fd);
    }
    tx_stamp = hal_time_ms();
    nrf24l01_set_prx(spi_fd);
  }

  if (nrf24l01_prx_pipe_available(spi_fd)==0) {
    rx_len = nrf24l01_prx_data(spi_fd, rx_buffer, NRF24_MTU);
    if (rx_len != 0) {
      printf("'%s' (%d):'%s'\n", MESSAGE, rx_len, rx_buffer);
      printf("Broadcasting...\n");
    }
  }
}
