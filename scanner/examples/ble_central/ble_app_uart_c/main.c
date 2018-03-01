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

#include "loopback.h"
#include "w5500.h"
#include "socket.h"
#include "dhcp.h"
#include "dhcp_cb.h"
#include "scan.h"

#define UART_TX_BUF_SIZE                    256                     /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                    256                     /**< UART RX buffer size. */

APP_TIMER_DEF(tcp_con_timer_id);									/**< Publish data timer. */
#define TCP_CON_INTERVAL             	    APP_TIMER_TICKS(1000)	/**< RR interval interval (ticks). */

APP_TIMER_DEF(tcp_socket_check_timer_id);							/**< Publish data timer. */
#define TCP_SOCKET_CHECK_INTERVAL         	APP_TIMER_TICKS(100)	/**< RR interval interval (ticks). */

#define DHCP_ENABLED                        1
#define _MAIN_DEBUG_                        0
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

#define LED_0                               11
#define LED_1                               12
#define LED_HP                              13
#define SYNC_OUT                            14    
#define SYNC_IN                             15
#define BUTTON_0                            16


#define PPI_CHANNEL_LED                     1
#define PPI_CHANNEL_SYNC                    0
#define SCAN_TIMER_MS                       500

#define SERVER_IP_PREFIX                    "position_server: "

#define LOG(...)                             printf(__VA_ARGS__) //NRF_LOG_RAW_INFO(__VA_ARGS__)

void connection_init(void);

static uint8_t TX_BUF[TX_BUF_SIZE];
static volatile bool data_in_flag           = false;
static volatile bool network_is_busy        = false;
static volatile bool connected              = false;

void send_to_tcp(uint8_t * buf, uint8_t length);

uint8_t target_IP[4] = {10, 0, 0, 12};
uint32_t target_port = 15000;




static volatile uint32_t flag = 0;
static volatile uint32_t sync_time = 0;
static volatile uint32_t capture_time = 0;
static volatile uint32_t diff = 0;



void on_connect(void)
{
    connected = true;
    nrf_gpio_pin_clear(LED_1);
}

void on_disconnect(void)
{
    connected = true;
    nrf_gpio_pin_set(LED_1);
}

void leds_init(void)
{
    nrf_gpio_cfg_output(LED_0);
    nrf_gpio_cfg_output(LED_1);
    nrf_gpio_cfg_output(LED_HP);
    
    nrf_gpio_pin_set(LED_0);
    nrf_gpio_pin_set(LED_1);
    nrf_gpio_pin_clear(LED_HP);
}


void gpiote_init(void) 
{
    // GPIOTE configuration for syncing of clocks
    NRF_GPIOTE->CONFIG[0]           = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos)
                                    | (SYNC_IN << GPIOTE_CONFIG_PSEL_Pos)
                                    | (0 << GPIOTE_CONFIG_PORT_Pos)
                                    | (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);

    // GPIOTE configuration for LED pin
    NRF_GPIOTE->CONFIG[1]           = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                    | (LED_0 << GPIOTE_CONFIG_PSEL_Pos)
                                    | (0 << GPIOTE_CONFIG_PORT_Pos)
                                    | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                    | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);

//    // GPIOTE configuration for LED pin
//    NRF_GPIOTE->CONFIG[2]           = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
//                                    | (LED2_PIN << GPIOTE_CONFIG_PSEL_Pos)
//                                    | (0 << GPIOTE_CONFIG_PORT_Pos)
//                                    | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
//                                    | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);


//    NRF_GPIOTE->INTENSET            = GPIOTE_INTENSET_IN0_Set << GPIOTE_INTENSET_IN0_Pos;
    //NVIC_ClearPendingIRQ(GPIOTE_IRQn);
    //NVIC_EnableIRQ(GPIOTE_IRQn);

}

void ppi_init(void) 
{
//    // PPI channel for syncing clocks and toggling LED on timer compare event
//    NRF_PPI->CH[PPI_CHANNEL_LED].EEP        = (uint32_t) &(NRF_TIMER1->EVENTS_COMPARE[0]);
//    NRF_PPI->CH[PPI_CHANNEL_LED].TEP        = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[0]);
//    NRF_PPI->FORK[PPI_CHANNEL_LED].TEP      = (uint32_t) &(NRF_TIMER2->TASKS_CAPTURE[0]);
//    NRF_PPI->CHENSET                        = 1 << PPI_CHANNEL_LED;

    NRF_PPI->CH[PPI_CHANNEL_SYNC].EEP       = (uint32_t) &(NRF_GPIOTE->EVENTS_IN[0]);
    NRF_PPI->CH[PPI_CHANNEL_SYNC].TEP       = (uint32_t) &(SCAN_TIMER->TASKS_CLEAR);
    NRF_PPI->FORK[PPI_CHANNEL_SYNC].TEP     = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[1]);
    NRF_PPI->CHENSET                        = 1 << PPI_CHANNEL_SYNC;

}

void timer_init(void) 
{
    // Timer for sampling of RSSI values
    NRF_TIMER1->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER1->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER1->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER1->CC[0]               = SCAN_TIMER_MS * 1000;                                                         // Compare event every 2 seconds
    NRF_TIMER1->SHORTS              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event
    NRF_TIMER1->INTENSET            = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;               // Enable interrupt for compare event
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_EnableIRQ(TIMER1_IRQn);

    // Timer for creating timestamps on RSSI capture
    NRF_TIMER2->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER2->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER2->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER2->CC[0]               = 0;

    // Timer for sync testing
    NRF_TIMER3->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER3->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER3->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER3->CC[0]               = 0;


    NRF_TIMER1->TASKS_START         = 1;
    NRF_TIMER2->TASKS_START         = 1;
    NRF_TIMER3->TASKS_START         = 1;
}

void TIMER3_IRQHandler(void) 
{
    NRF_TIMER1->EVENTS_COMPARE[0]   = 0;
    flag = 1;
}

void calc_time_since_sync(void)
{
    NRF_TIMER2->TASKS_CAPTURE[0] = 1;
    capture_time = NRF_TIMER2->CC[0];
    diff = capture_time - sync_time;
}

void GPIOTE_IRQHandler(void)
{
    NRF_GPIOTE->EVENTS_IN[1] = 0;
    sync_time = NRF_TIMER3->CC[0];
}


/**@brief Function for initializing the nrf log module. */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

void send_to_tcp(uint8_t * buf, uint8_t length)
{
    int32_t ret;
	
    if (getSn_SR(SOCKET_TCPC) == SOCK_ESTABLISHED)
    {
                    
        ret = send(SOCKET_TCPC,  (void *)buf, length);  // Data send process (User's buffer -> Destination through H/W Tx socket buffer)
        if(ret < 0)             // Send Error occurred (sent data length < 0)
            close(SOCKET_TCPC);   // socket close
    }
}

void tcp_socket_check(void * p_context)
{
    int32_t ret;
    uint32_t size;
    char buf[DATA_BUF_SIZE];
    
    if(!network_is_busy)
        {
        if((size = getSn_RX_RSR(SOCKET_TCPC)) > 0) // Sn_RX_RSR: Socket n Received Size Register, Receiving data length
        {
            ret = recv(SOCKET_TCPC, (uint8_t *)buf, size); // Data Receive process (H/W Rx socket buffer -> User's buffer)
            if (ret<0)
            {
                close(SOCKET_TCPC); // socket close
            }
            else
            {
                // Handle received data
                char str[size];
                memcpy((void *)&str[0], (void *)&buf[0], size);
                LOG("Received: %s\r\n", str);
            }
        }
    }
}

void tcp_con_timer(void * p_context)
{
    int32_t ret; // return value for SOCK_ERRORs

   // Port number for TCP client (will be increased)
    uint16_t any_port = 	50000;
    if(!network_is_busy)
    {

       // Socket Status Transitions
       // Check the W5500 Socket n status register (Sn_SR, The 'Sn_SR' controlled by Sn_CR command or Packet send/recv status)
        switch(getSn_SR(SOCKET_TCPC))
        {
            case SOCK_CLOSE_WAIT :
                if((ret=disconnect(SOCKET_TCPC)) != SOCK_OK) printf("%d:Socket Closed Fail\r\n", SOCKET_TCPC);
                connected = false;
                on_disconnect();
                connection_init();
                break;

            case SOCK_INIT :
                if( (ret = connect(SOCKET_TCPC, target_IP, target_port)) != SOCK_OK) 
                                LOG("%d:Socket Connect Fail\r\n", SOCKET_TCPC);	//	Try to TCP connect to the TCP server (destination)
                ret = send(SOCKET_TCPC,  (void *)"connected\r\n", 11);
                if (ret > -1)
                {
                    LOG("Successfully connected to %d.%d.%d.%d : %d\r\n", 
                            target_IP[0], target_IP[1], target_IP[2], target_IP[3], target_port);
                    app_timer_stop(tcp_con_timer_id);
                    on_connect();
                }
            break;

            case SOCK_CLOSED:
                close(SOCKET_TCPC);
                if((ret=socket(SOCKET_TCPC, Sn_MR_TCP, any_port++, 0x00)) != SOCKET_TCPC) 
                                LOG("%d:Socket Open Fail\r\n", SOCKET_TCPC); // TCP socket open with 'any_port' port number
                connected = false;
                on_disconnect();
                connection_init();
                break;
                            
            default:
                break;
        }
        
        network_is_busy = false;
    }
}

static void user_app_timer_init(void)
{
    uint32_t err_code = NRF_SUCCESS;
    
    err_code = app_timer_create(&tcp_con_timer_id, APP_TIMER_MODE_REPEATED, tcp_con_timer);
    APP_ERROR_CHECK(err_code);
	
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
    NRF_TIMER1->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER1->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER1->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER1->CC[0]               = 1000000;                                                         // Compare event every 2 seconds
    NRF_TIMER1->SHORTS              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event
    NRF_TIMER1->INTENSET            = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;               // Enable interrupt for compare event
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_EnableIRQ(TIMER1_IRQn);
    
    NRF_TIMER1->TASKS_START = 1;

}

void TIMER1_IRQHandler(void)
{
    NRF_TIMER1->EVENTS_COMPARE[0] = 0;
    DHCP_time_handler();
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

/* Function to send scan reports over Ethernet, using TCP or UDP */
void send_scan_report(scan_report_t * scan_report)
{
    uint8_t buf[150];
    uint8_t len = 0;
    
    if(!network_is_busy)
    {
        network_is_busy = true;
        
        sprintf((char *)&buf[0], "{ \"timestamp\" : %d, \"address\" : \"%02x:%02x:%02x:%02x:%02x:%02x\", \"RSSI\" : %d, \"channel\" : %d, \"CRC\" : %01d, \"LPE\" : %01d }\r\n", 
                        scan_report->timestamp, scan_report->address[0], scan_report->address[1], scan_report->address[2], 
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

void sync_init(void) 
{
    nrf_gpio_cfg_input(SYNC_IN, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_output(SYNC_OUT);
    nrf_gpio_pin_clear(SYNC_OUT);
}


void multicast_init(void) 
{
    uint8_t mac_addr[] = {0x01, 0x00, 0x5e, 0x01, 0x01, 0x0b};
    uint8_t ip_addr[] = {211, 1, 1, 11};
    setSn_DHAR(SOCKET_MULTICAST, &(mac_addr[0]));
    setSn_DIPR(SOCKET_MULTICAST, &(ip_addr[0]));
    setSn_DPORT(SOCKET_MULTICAST, 3000);
    setSn_PORT(SOCKET_MULTICAST, 3000);
    setSn_MR(SOCKET_MULTICAST, (Sn_MR_UDP | Sn_MR_MULTI));
    setSn_CR(SOCKET_MULTICAST , Sn_CR_OPEN);
    
    while (getSn_SR(SOCKET_MULTICAST) != SOCK_UDP) { printf("%d\r\n", getSn_SR(SOCKET_MULTICAST)); }
}

void broadcast_init(void)
{
    socket(SOCKET_BROADCAST, Sn_MR_UDP, BROADCAST_PORT, 0x00);
}

void broadcast_send(void) 
{
    uint8_t buf[] = {0x41, 0x42};
    uint8_t len = 2;
    uint8_t broadcast_ip[] = {255, 255, 255, 255};
    uint16_t broadcast_port = 10000;
    
    sendto(SOCKET_UDP, &buf[0], len, broadcast_ip, broadcast_port);
}

uint8_t get_server_ip(void)
{
    if ( getSn_RX_RSR(SOCKET_BROADCAST) != 0x00)
    {
        uint8_t len = 200;
        uint8_t buf[len];
        uint8_t broadcast_ip[] = {255, 255, 255, 255};
        uint16_t broadcast_port = BROADCAST_PORT;
        
        int32_t recv_len = recvfrom(SOCKET_BROADCAST, buf, len, broadcast_ip, &broadcast_port);
        
        if (recv_len > -1)
        {
            uint8_t prefix_len = strlen(SERVER_IP_PREFIX);
            uint8_t str[prefix_len];
            const uint8_t pos[] = SERVER_IP_PREFIX;
            strncpy((char *)str, (const char *)buf, prefix_len);
            if (!strcmp((char *)str, (const char *)pos))
            {
                // Server IP address prefix is found
                uint8_t ip_str[recv_len - prefix_len];
                uint8_t ip[4] = {0};
                uint8_t index = 0;
                uint8_t *p_ip_str = &(ip_str[0]);
                
                memcpy((char *)ip_str, (const char *)&(buf[prefix_len]), (recv_len - prefix_len));
                
                while (index < 4) 
                {
                    if (isdigit((unsigned char)*p_ip_str)) {
                        ip[index] *= 10;
                        ip[index] += *p_ip_str - '0';
                    } else {
                        index++;
                    }
                    p_ip_str += 1;
                }
                memcpy((char *)target_IP, (const char *)ip, 4);
                printf("Server IP: %d.%d.%d.%d\r\n", target_IP[0], target_IP[1], target_IP[2], target_IP[3]);
                return NRF_SUCCESS;
            }
        }
        else 
        {
            printf("No new UDP broadcast\r\n");
        }
    }
    return 1;
}

int main(void)
{   
    static scan_report_t scan_reports[3];
    
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
    //multicast_init();
    broadcast_init();
    while(get_server_ip() != NRF_SUCCESS);
    connection_init();
    scan_init();
    
    for (;;)
    {
        if (connected)
        {
            scan_ble_adv_channels_once(scan_reports);
            send_scan_report(&scan_reports[0]);
            send_scan_report(&scan_reports[1]);
            send_scan_report(&scan_reports[2]);
        }
}
}
