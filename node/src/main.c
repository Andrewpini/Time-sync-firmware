/**
 * Copyright (c) 2016 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "app_timer.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

#include "nrf_drv_spi.h"
#include "user_ethernet.h"
#include "user_spi.h"


#include "w5500.h"
#include "socket.h"
#include "dhcp.h"
#include "dhcp_cb.h"

#include "radio.h"
#include "scan.h"
#include "advertise.h"

#include "config.h"
#include "util.h"
#include "commands.h"
#include "timer.h"
#include "gpio.h"
#include "command_system.h"
#include "pwm.h"
#include "ppi.h"
#include "clock.h"
#include "timer_drift_measurement.h"

#define LOG(...)                            printf(__VA_ARGS__)         //NRF_LOG_RAW_INFO(__VA_ARGS__)
#define SOCKET_UDP                          3
#define SOCKET_BROADCAST                    6
#define UDP_PORT                            17545
#define UDP_FLAGS                           0x00
#define BROADCAST_PORT                      10000

/* Forward declarations */
void connection_init(void);
void on_connect(void);

/* State variables */
static volatile bool m_connected              = false;
static volatile uint32_t m_sync_time          = 0;


/**@brief Function for initializing the nrf log module. */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/// ***  SECTION: Network functions   *** ///

// Function to establish UDP socket 
void connection_init(void) 
{
        socket(SOCKET_UDP, Sn_MR_UDP, UDP_PORT, UDP_FLAGS);
        on_connect();
}

// Initiates socket for UDP broadcast messages
void broadcast_init(void)
{
    uint8_t flag = SF_IO_NONBLOCK;
    socket(SOCKET_BROADCAST, Sn_MR_UDP, BROADCAST_PORT, flag);
}

// Function to broadcast data to all nodes on the network
void broadcast_send(uint8_t * buf, uint8_t len) 
{
    uint8_t broadcast_ip[] = {255, 255, 255, 255};
    uint16_t broadcast_port = 10000;
    
    sendto(SOCKET_UDP, &buf[0], len, broadcast_ip, broadcast_port);
}

/// ***  SECTION: Event handlers   *** ///

void on_connect(void)
{
    m_connected = true;
    pwm_set_duty_cycle(LED_HP, LED_HP_DEFAULT_DUTY_CYCLE);
}

void on_disconnect(void)
{
    m_connected = true;
    pwm_set_duty_cycle(LED_HP, 0);
}

/// ***   SECTION: Helper functions   ***   ///

// Initializes time synchronization
void sync_init(void) 
{
    nrf_gpio_cfg_input(SYNC_IN, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_output(SYNC_OUT);
    nrf_gpio_pin_clear(SYNC_OUT);
}


void TIMER1_IRQHandler(void)
{
    NRF_TIMER1->EVENTS_COMPARE[0] = 0;
    DHCP_time_handler();
}

int main(void)
{   

    clock_init();
    leds_init();
    sync_init();
    gpiote_init();
    ppi_init();
    log_init();
    spi0_master_init();
    user_ethernet_init();
    drift_timer_init();
    dhcp_init();
    broadcast_init();
    LOG("\r\n\r\nApplication started.\r\n");

    while(!is_server_ip_received())
    {
        check_ctrl_cmd();
    }
    LOG("\r\n\r\nIP recieved.\r\n");

    connection_init();
    scan_init();
    
    for (;;)
    {
        if (m_connected)
        {
            if (is_server_ip_received())
            {
                send_drift_timing_sample();
            }
            check_ctrl_cmd();
        }
    }
}
