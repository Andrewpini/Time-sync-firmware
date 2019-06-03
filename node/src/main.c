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


#define LOG(...)                            printf(__VA_ARGS__)         //NRF_LOG_RAW_INFO(__VA_ARGS__)

#define UART_TX_BUF_SIZE                    256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                    256                         /**< UART RX buffer size. */

APP_TIMER_DEF(tcp_con_timer_id);									/**< Publish data timer. */
#define TCP_CON_INTERVAL             	    APP_TIMER_TICKS(1000)	/**< RR interval interval (ticks). */

APP_TIMER_DEF(tcp_socket_check_timer_id);							/**< Publish data timer. */
#define TCP_SOCKET_CHECK_INTERVAL           APP_TIMER_TICKS(100)	/**< RR interval interval (ticks). */

#define DHCP_ENABLED                        1
#define NETWORK_USE_TCP                     0
#define NETWORK_USE_UDP                     1

#define PRINT_SCAN_REPORTS                  1
#define PRINT_SCAN_GRAPH_DATA               0

#define SOCKET_CONFIG                       0
#define SOCKET_TCPC                         1
#define SOCKET_DHCP                         2
#define SOCKET_UDP                          3
#define SOCKET_MULTICAST                    4
#define SOCKET_BROADCAST                    6

#define UDP_PORT                            17545
#define UDP_FLAGS                           0x00

#define MULTICAST_PORT                      3000
#define BROADCAST_PORT                      10000

#define TX_BUF_SIZE                         2048



void connection_init(void);

// State variables
static uint8_t TX_BUF[TX_BUF_SIZE];
static volatile bool data_in_flag           = false;
static volatile bool network_is_busy        = false;
static volatile bool connected              = false;
static volatile bool server_ip_received     = false;
static volatile bool scanning_enabled       = true;
static volatile bool advertising_enabled    = false;
static volatile bool controls_sync_signal   = false;
static volatile bool who_am_i_enabled       = false;

void on_connect(void);

// Default / placeholder IP and port, will receive updated IP and port from server before sending any data
static uint8_t own_MAC[6]          = {0};
static uint8_t own_IP[4]           = {0};
static uint8_t target_IP[4]                 = {10, 0, 0, 4};      // Arbitrary fallback
static uint32_t target_port                 = 15000;

static volatile uint32_t flag               = 0;
static volatile uint32_t sync_time          = 0;
static volatile uint32_t capture_time       = 0;
static volatile uint32_t diff               = 0;
static volatile float led_hp_default_value  = LED_HP_CONNECTED_DUTY_CYCLE;
static volatile uint32_t sync_interval      = SYNC_INTERVAL_MS;

/* Variables for calculating drift */
static volatile int m_prev_counter_val;
static volatile int m_prev_drift;
static volatile int m_current_drift;
static volatile uint32_t m_time_tic;
static volatile bool m_updated_drift_rdy;

void leds_init(void)
{
    nrf_gpio_cfg_output(LED_0);
    nrf_gpio_cfg_output(LED_1);
    nrf_gpio_cfg_output(LED_HP);
    
    nrf_gpio_pin_clear(LED_1);
    nrf_gpio_pin_clear(LED_HP);
}

/**@brief Function for initialization oscillators.
 */
void clock_init()
{
    if(NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
    {
        /* Start 16 MHz crystal oscillator */
        NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
        NRF_CLOCK->TASKS_HFCLKSTART    = 1;

        /* Wait for the external oscillator to start up */
        while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
        {
            // Do nothing.
        }
    }
    
    if(NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
    {
        /* Start 32 kHz crystal oscillator */
        NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
        NRF_CLOCK->TASKS_LFCLKSTART    = 1;

        /* Wait for the external oscillator to start up */
        while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
        {
            // Do nothing.
        }
    }
}


/**@brief Function for initializing the nrf log module. */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/// ***  SECTION: Network functions   *** ///

// Function to establish socket, UDP or TCP depending on configuration
void connection_init(void) 
{
    #if NETWORK_USE_UDP
        socket(SOCKET_UDP, Sn_MR_UDP, UDP_PORT, UDP_FLAGS);
        on_connect();
    #endif
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
    connected = true;
    pwm_set_duty_cycle(LED_HP, LED_HP_DEFAULT_DUTY_CYCLE);
}

void on_disconnect(void)
{
    connected = true;
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


int calc_drift_time(uint32_t counter_val){
    int temp_diff = m_prev_counter_val - counter_val;
    if(temp_diff < - (DRIFT_TIMER_MAX / 2)){
      return DRIFT_TIMER_MAX - abs(temp_diff);
    } else {
      return temp_diff;
    }
}

void GPIOTE_IRQHandler(void)
{
    if (NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_SYNC_IN])
    {
      NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_SYNC_IN] = 0;
      sync_time = NRF_TIMER3->CC[0];

    /*TEST FOR Å SJEKKE OM PPI-CONNECTION MELLOM GPIO OG OG TIMER0 FUNGERER SOM DEN SKAL*/
    m_current_drift = m_prev_drift - calc_drift_time(NRF_TIMER0->CC[1]);
    LOG("Raw counter value : %d\n", NRF_TIMER0->CC[1]);
    LOG("Current timer drift (in relation to sync master) : %d microseconds\n", m_current_drift);
    m_time_tic++;
    m_prev_drift = m_current_drift;
    m_updated_drift_rdy = true;

    m_prev_counter_val = NRF_TIMER0->CC[1];
    }
}

void TIMER1_IRQHandler(void)
{
    NRF_TIMER1->EVENTS_COMPARE[0] = 0;
    DHCP_time_handler();
}

///   ***   ///

int main(void)
{   
//    uint8_t err_code_37 = SUCCESS;
//    uint8_t err_code_38 = SUCCESS;
//    uint8_t err_code_39 = SUCCESS;
//    static scan_report_t scan_reports[3];
//    static uint32_t counter = 1;
    
    clock_init();
    leds_init();
    sync_init();
    gpiote_init();
    ppi_init();
    log_init();

    LOG("\r\n\r\nApplication started.\r\n");
    spi0_master_init();
    user_ethernet_init();
    
    drift_timer_init();
    dhcp_init();

    broadcast_init();
    while(!server_ip_received)
    {
        check_ctrl_cmd();
    }

    connection_init();
    scan_init();
    
    for (;;)
    {
        if (connected)
        {
            if (is_server_ip_received())
            {
                if(m_updated_drift_rdy){
                    int outgoing_drift = m_current_drift;
                    int outgoing_time_tic = m_time_tic;
                    send_timing_samples(m_current_drift, m_time_tic);
                    m_updated_drift_rdy = false;
                }
//                if (is_scanning_enabled())
//                {
//                    //scan_ble_adv_channels_once(scan_reports);
//                    err_code_37 = scan_ble_channel_once(&scan_reports[0], 37);
//                    err_code_38 = scan_ble_channel_once(&scan_reports[1], 38);
//                    err_code_39 = scan_ble_channel_once(&scan_reports[2], 39);
//
//                    if (err_code_37 == SUCCESS)
//                        send_scan_report(&scan_reports[0]);
//                    if (err_code_38 == SUCCESS)
//                        send_scan_report(&scan_reports[1]);
//                    if (err_code_39 == SUCCESS)
//                        send_scan_report(&scan_reports[2]);
//                }
//                if (is_advertising_enabled())
//                {   
//                    advertise_set_payload((uint8_t *)&counter, sizeof(counter));
//                    nrf_delay_ms(ADVERTISING_INTERVAL);
//                    advertise_ble_channel_once(37);   
//                    advertise_ble_channel_once(38);   
//                    advertise_ble_channel_once(39);
//                    counter++;      
//                }
//                if (is_who_am_i_enabled())
//                {
//                    send_who_am_i();
//                }
            }
            check_ctrl_cmd();
        }
    }
}
