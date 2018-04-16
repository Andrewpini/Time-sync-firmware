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


#define LOG(...)                             printf(__VA_ARGS__)    //NRF_LOG_RAW_INFO(__VA_ARGS__)

#define UART_TX_BUF_SIZE                    256                     /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                    256                     /**< UART RX buffer size. */

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
#define SOCKET_BROADCAST                    5

#define UDP_PORT                            17545
#define UDP_FLAGS                           0x00

#define MULTICAST_PORT                      3000
#define BROADCAST_PORT                      10000

#define TX_BUF_SIZE                         2048

#define LED_0                               11UL
#define LED_1                               12UL
#define LED_HP                              13UL
#define SYNC_OUT                            14UL  
#define SYNC_IN                             15UL
#define BUTTON_0                            16UL


#define SCAN_TIMER_MS                       500


#define SCAN_REPORT_LENGTH                  200
#define SERVER_IP_PREFIX                    "position_server: "
#define SERVER_IP_PREFIX_LEN                17

#define PWM_PERIOD_US                       1000



void connection_init(void);

static uint8_t TX_BUF[TX_BUF_SIZE];
static volatile bool data_in_flag           = false;
static volatile bool network_is_busy        = false;
static volatile bool connected              = false;
static volatile bool server_ip_received     = false;
static volatile bool scanning_enabled       = true;
static volatile bool advertising_enabled    = false;

void send_to_tcp(uint8_t * buf, uint8_t length);
void on_connect(void);

// Default / placeholder IP and port, will receive updated IP and port from server before sending any data
uint8_t own_IP[4] = {0};
uint8_t target_IP[4] = {10, 0, 0, 4};      
uint32_t target_port = 15000;

static volatile uint32_t flag = 0;
static volatile uint32_t sync_time = 0;
static volatile uint32_t capture_time = 0;
static volatile uint32_t diff = 0;
static volatile uint16_t pwm_seq[1] = {0};
static volatile float led_hp_default_value = LED_HP_CONNECTED_DUTY_CYCLE;


void leds_init(void)
{
    nrf_gpio_cfg_output(LED_0);
    nrf_gpio_cfg_output(LED_1);
    nrf_gpio_cfg_output(LED_HP);
    
    nrf_gpio_pin_clear(LED_1);
    nrf_gpio_pin_clear(LED_HP);
}


// ***  SECTION: Peripheral modules   *** //


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


/**@brief Function for initialization of GPIOTE
 */
void gpiote_init(void) 
{
    // GPIOTE configuration for syncing of clocks
    NRF_GPIOTE->CONFIG[GPIOTE_CHANNEL_SYNC_IN]  = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos)
                                                | (SYNC_IN << GPIOTE_CONFIG_PSEL_Pos)
                                                | (0 << GPIOTE_CONFIG_PORT_Pos)
                                                | (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);

    // GPIOTE configuration for LED pin
    NRF_GPIOTE->CONFIG[GPIOTE_CHANNEL_SYNC_LED] = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                                | (LED_0 << GPIOTE_CONFIG_PSEL_Pos)
                                                | (0 << GPIOTE_CONFIG_PORT_Pos)
                                                | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                                | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);
}

/**@brief Function for initialization of PPI.
 */
void ppi_init(void) 
{
    NRF_PPI->CH[PPI_CHANNEL_SYNC_IN].EEP        = (uint32_t) &(NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_SYNC_IN]);
    NRF_PPI->CH[PPI_CHANNEL_SYNC_IN].TEP        = (uint32_t) &(SCAN_TIMESTAMP_TIMER->TASKS_CLEAR);
    NRF_PPI->FORK[PPI_CHANNEL_SYNC_IN].TEP      = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[GPIOTE_CHANNEL_SYNC_LED]);
    NRF_PPI->CHENSET                            = 1 << PPI_CHANNEL_SYNC_IN;

}


/**@brief Sets PWM properties for a pin. Duty cycle in percent. Frequency is statically set to 1000 Hz.
*/
void pwm_set_duty_cycle(uint8_t pin, float duty_cycle)
{
    pwm_seq[0] = ( 1 << 15 ) | (uint16_t)(PWM_PERIOD_US * (uint32_t)(10.0 * duty_cycle) / 1000);
    NRF_PWM1->PSEL.OUT[0] = (pin << PWM_PSEL_OUT_PIN_Pos) | (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);
    NRF_PWM1->ENABLE = (PWM_ENABLE_ENABLE_Enabled << PWM_ENABLE_ENABLE_Pos);
    NRF_PWM1->MODE = (PWM_MODE_UPDOWN_Up << PWM_MODE_UPDOWN_Pos);
    NRF_PWM1->PRESCALER = (PWM_PRESCALER_PRESCALER_DIV_16 << PWM_PRESCALER_PRESCALER_Pos);
    NRF_PWM1->COUNTERTOP = (PWM_PERIOD_US << PWM_COUNTERTOP_COUNTERTOP_Pos); 
    NRF_PWM1->LOOP = (PWM_LOOP_CNT_Disabled << PWM_LOOP_CNT_Pos);
    NRF_PWM1->DECODER = (PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos) | (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);
    NRF_PWM1->SEQ[0].PTR = ((uint32_t)(pwm_seq) << PWM_SEQ_PTR_PTR_Pos);
    NRF_PWM1->SEQ[0].CNT = ((sizeof(pwm_seq) / sizeof(uint16_t)) << PWM_SEQ_CNT_CNT_Pos);
    NRF_PWM1->SEQ[0].REFRESH = 0;
    NRF_PWM1->SEQ[0].ENDDELAY = 0;
    NRF_PWM1->TASKS_SEQSTART[0] = 1;
}
//   ***   //


/**@brief Function for initializing the nrf log module. */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


static void user_app_timer_init(void)
{
    uint32_t err_code = NRF_SUCCESS;
    
    //err_code = app_timer_create(&tcp_con_timer_id, APP_TIMER_MODE_REPEATED, tcp_con_timer);
   // APP_ERROR_CHECK(err_code);
	
   // err_code = app_timer_create(&tcp_socket_check_timer_id, APP_TIMER_MODE_REPEATED, tcp_socket_check);
    //APP_ERROR_CHECK(err_code);	
}

static void user_app_timer_start(void)
{
    uint32_t err_code;
    
    err_code = app_timer_start(tcp_con_timer_id, TCP_CON_INTERVAL,NULL);
    APP_ERROR_CHECK(err_code);

    //err_code = app_timer_start(tcp_socket_check_timer_id, TCP_SOCKET_CHECK_INTERVAL, NULL);
    //APP_ERROR_CHECK(err_code);	
}

void dhcp_timer_init(void)
{
    DHCP_TIMER->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    DHCP_TIMER->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    DHCP_TIMER->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    DHCP_TIMER->CC[0]               = 1000000;                                                                      // Compare event every second
    DHCP_TIMER->SHORTS              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event
    DHCP_TIMER->INTENSET            = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;               // Enable interrupt for compare event
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_EnableIRQ(TIMER1_IRQn);
    
    DHCP_TIMER->TASKS_START = 1;

}

static void dhcp_init(void)
{
    if(DHCP_ENABLED && !network_is_busy)
    {
        uint32_t ret;
        uint8_t dhcp_retry = 0;
        network_is_busy = true;

        dhcp_timer_init();
        DHCP_init(SOCKET_DHCP, TX_BUF);
        reg_dhcp_cbfunc(w5500_dhcp_assign, w5500_dhcp_assign, w5500_dhcp_conflict);

        while(1)
        {
            ret = DHCP_run();

            if(ret == DHCP_IP_LEASED)
            {
                network_is_busy = false;
                getIPfromDHCP(&own_IP[0]);
                LOG("\r\n\r\nThis device' IP: %d.%d.%d.%d\r\n", own_IP[0], own_IP[1], own_IP[2], own_IP[3]);
                print_network_info();
                break;
            }
            else if(ret == DHCP_FAILED)
            {
                 dhcp_retry++;  
            }

            if(dhcp_retry > 10)
            {
                break;
            }
        }
        network_is_busy = false;
    }
    else 							
    {
        // Fallback config if DHCP fails
    }
}


/* Function to send scan reports over Ethernet, using TCP or UDP */
void send_scan_report(scan_report_t * scan_report)
{
    uint8_t buf[SCAN_REPORT_LENGTH];
    uint8_t len = 0;
    
    if(!network_is_busy)
    {
        network_is_busy = true;
        
        sprintf((char *)&buf[0], "{ \"nodeID\" : \"%02x:%02x:%02x:%02x:%02x:%02x\", \"timestamp\" : %d, \t \"counter\" : %d, \t \"address\" : \"%02x:%02x:%02x:%02x:%02x:%02x\", \"RSSI\" : %d, \"channel\" : %d, \"CRC\" : %01d, \"LPE\" : %01d }\r\n", 
                        scan_report->id[0], scan_report->id[1], scan_report->id[2], scan_report->id[3], scan_report->id[4], scan_report->id[5],
                        scan_report->timestamp, scan_report->counter, scan_report->address[0], scan_report->address[1], scan_report->address[2], 
                        scan_report->address[3], scan_report->address[4], scan_report->address[5],
                        scan_report->rssi, scan_report->channel, scan_report->crc_status, scan_report->long_packet_error);
        
        len = strlen((const char *)&buf[0]);
        
        #if NETWORK_USE_TCP
            send_to_tcp(&buf[0], len);
        #endif
        
        #if NETWORK_USE_UDP
            sendto(SOCKET_UDP, &buf[0], len, target_IP, target_port);
        #endif
        
        network_is_busy = false;
    }
}


/// ***  SECTION: Network functions   *** ///

// Function to establish socket, UDP or TCP depending on configuration
void connection_init(void) 
{
    #if NETWORK_USE_TCP
        user_app_timer_init();
        user_app_timer_start();
    #endif
    
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

// Function to get and store server IP and port number to which all data will be sent
void get_server_ip(uint8_t * buf, uint8_t len)
{
    static uint8_t str[SERVER_IP_PREFIX_LEN] = {0};
    const uint8_t pos[] = SERVER_IP_PREFIX;
    strncpy((void *)str, (const void *)buf, SERVER_IP_PREFIX_LEN);
    int8_t compare = strncmp((void *)str, (const void *)pos, SERVER_IP_PREFIX_LEN);

    if (compare == 0)
    {
        // Server IP address prefix is found
        server_ip_received = true;
        uint8_t ip_str[len - SERVER_IP_PREFIX_LEN];
        uint8_t ip[4] = {0};
        uint16_t port = 0;
        uint8_t index = 0;
        uint8_t *p_ip_str = &(ip_str[0]);

        memcpy((char *)ip_str, (const char *)&(buf[SERVER_IP_PREFIX_LEN]), (len - SERVER_IP_PREFIX_LEN));

        while (index < 5) 
        {
            if (isdigit((unsigned char)*p_ip_str)) {
                if (index < 4)
                {
                    ip[index] *= 10;
                    ip[index] += *p_ip_str - '0';
                }
                else
                {
                    port *= 10;
                    port += *p_ip_str - '0';
                }
            } else {
                index++;
            }
            p_ip_str += 1;
        }
        memcpy((char *)target_IP, (const char *)ip, 4);
        target_port = port;
        printf("Server IP: %d.%d.%d.%d : %d\r\n", target_IP[0], target_IP[1], target_IP[2], target_IP[3], target_port);

    }
}
///   ***   ///


/// ***  SECTION: Event handlers   *** ///

void on_connect(void)
{
    connected = true;
    pwm_set_duty_cycle(LED_HP, led_hp_default_value);
}

void on_disconnect(void)
{
    connected = true;
    pwm_set_duty_cycle(LED_HP, 0);
}

///   ***   ///



/// ***   SECTION: Helper functions   ***   ///

// Initializes time synchronization
void sync_init(void) 
{
    nrf_gpio_cfg_input(SYNC_IN, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_output(SYNC_OUT);
    nrf_gpio_pin_clear(SYNC_OUT);
}

// Enables scanning for current radio mode
void scanning_enable(void)
{
    scanning_enabled = true;
}

// Disables scanning for current radio mode
void scanning_disable(void)
{
    scanning_enabled = false;
}

// Enables advertising for current radio mode
void advertising_enable(void)
{                    
    advertise_init();
    advertising_enabled = true;
}

// Disables advertising for current radio mode
void advertising_disable(void)
{
    advertising_enabled = false;
}

// Set node as sync master
void sync_master_set(void)
{
    SYNC_TIMER->MODE                                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    SYNC_TIMER->BITMODE                             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    SYNC_TIMER->PRESCALER                           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    SYNC_TIMER->CC[0]                               = SYNC_INTERVAL_MS * 1000; 
    SYNC_TIMER->SHORTS                              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event

    // GPIOTE configuration for syncing of clocks
    NRF_GPIOTE->CONFIG[GPIOTE_CHANNEL_SYNC_OUT]     = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                                    | (SYNC_OUT << GPIOTE_CONFIG_PSEL_Pos)
                                                    | (0 << GPIOTE_CONFIG_PORT_Pos)
                                                    | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                                    | (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);

    // PPI channel configuration for triggering syncing
    NRF_PPI->CH[PPI_CHANNEL_SYNC_OUT].EEP           = (uint32_t) &(SYNC_TIMER->EVENTS_COMPARE[0]);
    NRF_PPI->CH[PPI_CHANNEL_SYNC_OUT].TEP           = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[GPIOTE_CHANNEL_SYNC_OUT]);
    NRF_PPI->CHENSET                                = 1 << PPI_CHANNEL_SYNC_OUT;

    SYNC_TIMER->TASKS_START                         = 1;
}

// Unset node as sync master
void sync_master_unset(void)
{
    SYNC_TIMER->TASKS_STOP          = 1;
}

///   ***   ///



/// ***   SECTION: Command system   ***   ///

// Function for checking if the device has received a new control command
void check_ctrl_cmd(void)
{
    while (getSn_RX_RSR(SOCKET_BROADCAST) != 0x00)
    {
        uint8_t received_data[200];
        uint8_t broadcast_ip[] = {255, 255, 255, 255};
        uint16_t broadcast_port = BROADCAST_PORT;
        
        // Receive new data from socket
        int32_t recv_len = recvfrom(SOCKET_BROADCAST, received_data, sizeof(received_data), &broadcast_ip[0], &broadcast_port);

        // If any data is received, check which command it is
        if (recv_len > 0)
        {
            uint8_t str[CTRL_CMD_PREFIX_LEN] = {0};
            const uint8_t pos[] = CTRL_CMD_PREFIX;
            strncpy((void *)str, (const void *)received_data, CTRL_CMD_PREFIX_LEN);

            // Check if command prefix is correct
            int8_t compare = strncmp((void *)str, (const void *)pos, CTRL_CMD_PREFIX_LEN);
            if (compare == 0)
            {
                ctrl_cmd_t cmd = received_data[CTRL_CMD_CMD_INDEX];
                uint8_t payload_len = received_data[CTRL_CMD_PAYLOAD_LEN_INDEX];
                uint8_t * p_payload = &received_data[CTRL_CMD_PAYLOAD_INDEX];

                // Choose the right action according to command
                switch (cmd)
                {
                    case CMD_SERVER_IP_BROADCAST:
                        if (!server_ip_received)
                        {
                            get_server_ip(p_payload, payload_len);
                        }
                        server_ip_received = true;
                        break;

                    case CMD_NEW_SERVER_IP:
                        server_ip_received = false;
                        break;

                    case CMD_NEW_FIRMWARE:
                        LOG("CMD: New firmware available\r\n");
                        break;

                    case CMD_NEW_ACCESS_ADDRESS:
                        LOG("CMD: New access address set\r\n");
                        radio_set_access_address((uint32_t)*((uint32_t *)p_payload));
                        break;

                    case CMD_ADVERTISING_START:
                        LOG("CMD: Advertising start\r\n");
                        advertising_enable();
                        break;

                    case CMD_ADVERTISING_STOP:
                        LOG("CMD: Advertising stop\r\n");
                        advertising_disable();
                        break;

                    case CMD_ALL_HPLED_ON:
                        LOG("CMD: All HP LEDs ON: \r\n");
                        pwm_set_duty_cycle(LED_HP, LED_HP_ON_DUTY_CYCLE);
                        break;

                    case CMD_ALL_HPLED_OFF:
                        LOG("CMD: All HP LEDs OFF \r\n");
                        pwm_set_duty_cycle(LED_HP, LED_HP_OFF_DUTY_CYCLE);
                        break;

                    case CMD_ALL_HPLED_DEFAULT:
                        LOG("CMD: Set all HP LEDs to default value  \r\n");
                        pwm_set_duty_cycle(LED_HP, led_hp_default_value);
                        break;

                    case CMD_ALL_HPLED_NEW_DEFAULT:
                        LOG("CMD: Sets new HP LED default value: %d\r\n", p_payload[0]);
                        led_hp_default_value = p_payload[0] + (p_payload[1] / 10.0f);
                        pwm_set_duty_cycle(LED_HP, led_hp_default_value);
                        break;

                    case CMD_ALL_HPLED_CUSTOM:
                        LOG("CMD: All HP LEDs set to value: %d.%d\r\n", p_payload[0], (p_payload[1] / 10.0f));
                        float duty_cycle = p_payload[0] + (p_payload[1] / 10.0f);
                        pwm_set_duty_cycle(LED_HP, duty_cycle);
                        break;

                    case CMD_SINGLE_HPLED_ON:
                        LOG("CMD: Single HP LED ON: ");
                        if (IPs_are_equal((uint8_t *)p_payload, own_IP))
                        {
                            pwm_set_duty_cycle(LED_HP, LED_HP_ON_DUTY_CYCLE);
                            LOG("IP match -> turning HP LED ON\r\n");
                        }
                        else 
                        {
                            LOG("no IP match -> no action \r\n");
                        }
                        break;

                    case CMD_SINGLE_HPLED_OFF:
                        LOG("CMD: Single HP LED OFF: ");
                        if (IPs_are_equal((uint8_t *)p_payload, own_IP))
                        {
                            pwm_set_duty_cycle(LED_HP, LED_HP_OFF_DUTY_CYCLE);
                            LOG("IP match -> turning HP LED OFF\r\n");
                        }
                        else 
                        {
                            LOG("no IP match -> no action\r\n");
                        }
                        break;

                    case CMD_SINGLE_HPLED_DEFAULT:
                        LOG("CMD: Single HP LED default: ");
                        if (IPs_are_equal((uint8_t *)p_payload, own_IP))
                        {
                            pwm_set_duty_cycle(LED_HP, led_hp_default_value);
                            LOG("IP match -> setting HP LED to default value\r\n");
                        }
                        else 
                        {
                            LOG("no IP match -> no action \r\n");
                        }
                        break;

                    case CMD_SINGLE_HPLED_CUSTOM:
                        LOG("CMD: Single HP LED custom value: ");
                        if (IPs_are_equal((uint8_t *)p_payload, own_IP))
                        {
                            float duty_cycle = p_payload [4] + (p_payload[5] / 10.0f);
                            pwm_set_duty_cycle(LED_HP, duty_cycle);
                            LOG("IP match -> setting HP LED duty cycle to %d.%d \% \r\n", p_payload[4], p_payload[5]);
                        }
                        else 
                        {
                            LOG("no IP match -> no action \r\n");
                        }
                        break;

                    case CMD_SINGLE_ADVERTISING_ON:
                        LOG("CMD: Single advertising START: ");
                        if (IPs_are_equal((uint8_t *)p_payload, own_IP))
                        {
                            scanning_disable();
                            advertising_enable();
                            LOG("IP match -> enabling advertising \r\n");
                        }
                        else 
                        {
                            LOG("no IP match -> no action \r\n");
                        }
                        break;

                    case CMD_SINGLE_ADVERTISING_OFF:
                        LOG("CMD: Single advertising STOP: ");
                        if (IPs_are_equal((uint8_t *)p_payload, own_IP))
                        {
                            advertising_disable();
                            scanning_enable();
                            LOG("IP match -> disabling advertising \r\n");
                        }
                        else 
                        {
                            LOG("no IP match -> no action \r\n");
                        }
                        break;

                     case CMD_SYNC_NODE_SET:
                        LOG("CMD: Sync node set: ");
                        if (IPs_are_equal((uint8_t *)p_payload, own_IP))
                        {
                            sync_master_set();
                            LOG("IP match -> setting node as sync master \r\n");
                        }
                        else 
                        {
                            sync_master_unset();
                            LOG("no IP match -> no action \r\n");
                        }
                        break;

                    default:
                        LOG("CMD: Unrecognized control command: %d\r\n", cmd);
                        break;
                }
            }
        }
    }
}

///   ***   ///


// ***  SECTION: Interrupt handlers  *** //

void GPIOTE_IRQHandler(void)
{
    NRF_GPIOTE->EVENTS_IN[1] = 0;
    sync_time = NRF_TIMER3->CC[0];
}


void TIMER1_IRQHandler(void)
{
    NRF_TIMER1->EVENTS_COMPARE[0] = 0;
    DHCP_time_handler();
}


///   ***   ///


int main(void)
{   
    uint8_t err_code_37 = SUCCESS;
    uint8_t err_code_38 = SUCCESS;
    uint8_t err_code_39 = SUCCESS;
    static scan_report_t scan_reports[3];
    static uint32_t counter = 1;
    
    clock_init();
    leds_init();
    sync_init();
    gpiote_init();
    ppi_init();
    log_init();

    LOG("\r\n\r\nApplication started.\r\n");
    spi0_master_init();
    user_ethernet_init();
    
    //ret_code_t err_code = app_timer_init();
    //APP_ERROR_CHECK(err_code);
    
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
            if (server_ip_received)
            {
                if (scanning_enabled)
                {
                    //scan_ble_adv_channels_once(scan_reports);
                    err_code_37 = scan_ble_channel_once(&scan_reports[0], 37);
                    err_code_38 = scan_ble_channel_once(&scan_reports[1], 38);
                    err_code_39 = scan_ble_channel_once(&scan_reports[2], 39);

                    if (err_code_37 == SUCCESS)
                        send_scan_report(&scan_reports[0]);
                    if (err_code_38 == SUCCESS)
                        send_scan_report(&scan_reports[1]);
                    if (err_code_39 == SUCCESS)
                        send_scan_report(&scan_reports[2]);
                }
                if (advertising_enabled)
                {   
                    advertise_set_payload((uint8_t *)&counter, sizeof(counter));
                    nrf_delay_ms(ADVERTISING_INTERVAL);
                    advertise_ble_channel_once(37);   
                    advertise_ble_channel_once(38);   
                    advertise_ble_channel_once(39);
                    counter++;      
                }
            }
            check_ctrl_cmd();
        }
    }
}
