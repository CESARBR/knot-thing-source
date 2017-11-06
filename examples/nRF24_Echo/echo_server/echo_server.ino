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

#define CH_BROADCAST          76
#define MAX_RT                50
#define DEV                   "/dev/spidev0.0"

static int32_t tx_stamp;
static char rx_buffer[NRF24_MTU], tx_buffer[NRF24_MTU];
static uint8_t spi_fd = io_setup(DEV);
static uint8_t broadcast_addr[5] = {0x8D, 0xD9, 0xBE, 0x96, 0xDE};
static int8_t rx_len = 0;

void setup()
{
  Serial.begin(115200);
  printf_serial_init();
  nrf24l01_init(DEV, NRF24_PWR_6DBM);
  nrf24l01_set_standby(spi_fd);
  nrf24l01_open_pipe(spi_fd, broadcast_addr, 0);
  nrf24l01_set_channel(spi_fd,0, CH_BROADCAST);
  nrf24l01_set_prx(spi_fd);
  printf("Listening!!!\n");
  tx_stamp = 0;
}

void loop()
{
  if (rx_len != 0 && (hal_time_ms() - tx_stamp) > 3) {
      memcpy(tx_buffer, rx_buffer, rx_len);
      nrf24l01_set_ptx(spi_fd, 0);
      if (nrf24l01_ptx_data(spi_fd, tx_buffer, rx_len) == 0) {
          nrf24l01_ptx_wait_datasent(spi_fd);
      }
      nrf24l01_set_prx(spi_fd);
      tx_stamp = hal_time_ms();
  }

  if (nrf24l01_prx_pipe_available(spi_fd)==0) {
    rx_len = nrf24l01_prx_data(spi_fd, rx_buffer, NRF24_MTU);
    if (rx_len != 0) {
      printf("RX[%d]:'%s'\n", rx_len, rx_buffer);
      memcpy(rx_buffer, MESSAGE, MESSAGE_SIZE-1);
    }
  }
}
