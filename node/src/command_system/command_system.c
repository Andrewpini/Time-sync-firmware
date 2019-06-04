#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "w5500.h"
#include "socket.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "commands.h"
#include "radio.h"
#include "advertise.h"
#include "gpio.h"
#include "pwm.h"
#include "scan.h"
#include "nrf_gpio.h"
#include "util.h"
#include "dhcp.h"
#include "timer.h"
#include "dhcp_cb.h"
#include "user_ethernet.h"
#include "timer_drift_measurement.h"

#define DHCP_ENABLED                        1
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

#define PWM_PERIOD_US                       1000
#define LOG(...)                            printf(__VA_ARGS__)         //NRF_LOG_RAW_INFO(__VA_ARGS__)

static uint8_t target_IP[4]                 = {10, 0, 0, 4};      // Arbitrary fallback
static uint32_t target_port                 = 15000;
static volatile float led_hp_default_value  = LED_HP_CONNECTED_DUTY_CYCLE;
static volatile uint32_t sync_interval      = SYNC_INTERVAL_MS;
static uint8_t TX_BUF[TX_BUF_SIZE];
static uint8_t own_MAC[6]          = {0};
static uint8_t own_IP[4]           = {0};


static volatile bool m_network_is_busy        = false;
static volatile bool m_controls_sync_signal   = false;
static volatile bool m_scanning_enabled       = true;
static volatile bool m_advertising_enabled    = false;
static volatile bool m_who_am_i_enabled       = false;
static volatile bool m_server_ip_received     = false;


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
        m_server_ip_received = true;
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

// Enables scanning for current radio mode
void scanning_enable(void)
{
    m_scanning_enabled = true;
}

// Disables scanning for current radio mode
void scanning_disable(void)
{
    m_scanning_enabled = false;
}

// Enables advertising for current radio mode
void advertising_enable(void)
{                    
    advertise_init();
    m_advertising_enabled = true;
}

// Disables advertising for current radio mode
void advertising_disable(void)
{
    m_advertising_enabled = false;
}

// Set node as sync master
void sync_master_set(uint32_t interval)
{
    SYNC_TIMER->TASKS_STOP                          = 1;
    SYNC_TIMER->MODE                                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    SYNC_TIMER->BITMODE                             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    SYNC_TIMER->PRESCALER                           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    SYNC_TIMER->CC[0]                               = interval * 1000; 
    SYNC_TIMER->SHORTS                              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event


    nrf_gpio_cfg_output(SYNC_IN);

    // GPIOTE configuration for syncing of clocks
    NRF_GPIOTE->CONFIG[GPIOTE_CHANNEL_SYNC_OUT]     = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                                    | (SYNC_IN << GPIOTE_CONFIG_PSEL_Pos)
                                                    | (0 << GPIOTE_CONFIG_PORT_Pos)
                                                    | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                                    | (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);

    // PPI channel configuration for triggering syncing
    NRF_PPI->CH[PPI_CHANNEL_SYNC_OUT].EEP           = (uint32_t) &(SYNC_TIMER->EVENTS_COMPARE[0]);
    NRF_PPI->CH[PPI_CHANNEL_SYNC_OUT].TEP           = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[GPIOTE_CHANNEL_SYNC_OUT]);
    NRF_PPI->CHENSET                                = 1 << PPI_CHANNEL_SYNC_OUT;

    SYNC_TIMER->TASKS_START                         = 1;
    m_controls_sync_signal                            = true;
}

// Unset node as sync master
void sync_master_unset(void)
{
    SYNC_TIMER->TASKS_STOP          = 1;
    m_controls_sync_signal            = false;
}

// Sets interval in units of 100 ms
void sync_set_interval(uint8_t interval)
{
    sync_interval = interval * 100;
    sync_master_set(sync_interval);
}

/* Function to send scan reports over Ethernet, using TCP or UDP */
void send_scan_report(scan_report_t * scan_report)
{
    uint8_t buf[SCAN_REPORT_LENGTH];
    uint8_t len = 0;
    
    if(!m_network_is_busy)
    {
        m_network_is_busy = true;
        
        sprintf((char *)&buf[0], "{ \"nodeID\" : \"%02x:%02x:%02x:%02x:%02x:%02x\", \"timestamp\" : %d, \t \"counter\" : %d, \t \"address\" : \"%02x:%02x:%02x:%02x:%02x:%02x\", \"RSSI\" : %d, \"channel\" : %d, \"CRC\" : %01d, \"LPE\" : %01d, \"syncController\" : %01d }\r\n", 
                        scan_report->id[0], scan_report->id[1], scan_report->id[2], scan_report->id[3], scan_report->id[4], scan_report->id[5],
                        scan_report->timestamp, scan_report->counter, scan_report->address[0], scan_report->address[1], scan_report->address[2], 
                        scan_report->address[3], scan_report->address[4], scan_report->address[5],
                        scan_report->rssi, scan_report->channel, scan_report->crc_status, scan_report->long_packet_error, m_controls_sync_signal);
        
        len = strlen((const char *)&buf[0]);
        
        
        #if NETWORK_USE_UDP
            sendto(SOCKET_UDP, &buf[0], len, target_IP, target_port);
        #endif
        
        m_network_is_busy = false;
    }
}


void dhcp_init(void)
{
    if(DHCP_ENABLED && !m_network_is_busy)
    {
        uint32_t ret;
        uint8_t dhcp_retry = 0;
        m_network_is_busy = true;

        dhcp_timer_init();
        DHCP_init(SOCKET_DHCP, TX_BUF);
        reg_dhcp_cbfunc(w5500_dhcp_assign, w5500_dhcp_assign, w5500_dhcp_conflict);

        while(1)
        {
            ret = DHCP_run();

            if(ret == DHCP_IP_LEASED)
            {
                m_network_is_busy = false;
                getSHAR(&own_MAC[0]);
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
        m_network_is_busy = false;
    }
    else 							
    {
        // Fallback config if DHCP fails
    }
}

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
                    case WHOAMI_START:
                        LOG("CMD: WHO AM I start\r\n");
                        m_who_am_i_enabled = true;
                        break;

                    case WHOAMI_STOP:
                        LOG("CMD: WHO AM I stop\r\n");
                        m_who_am_i_enabled = false;
                        break;

                    case CMD_SERVER_IP_BROADCAST:
                        if (!m_server_ip_received)
                        {
                            get_server_ip(p_payload, payload_len);
                        }
                        m_server_ip_received = true;
                        break;

                    case CMD_NEW_SERVER_IP:
                        m_server_ip_received = false;
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
                            sync_master_set(sync_interval);
                            LOG("IP match -> setting node as sync master \r\n");
                        }
                        else 
                        {
                            sync_master_unset();
                            LOG("no IP match -> unset as sync master, no further action \r\n");
                        }
                        break;
                    
                    case CMD_SYNC_SET_INTERVAL:
                        LOG("CMD: Sync set interval: ");
                        if (IPs_are_equal((uint8_t *)p_payload, own_IP))
                        {
                            sync_set_interval(p_payload[4]);
                            LOG("IP match -> setting sync interval to %d ms\r\n", p_payload[4] * 100);
                        }
                        else 
                        {
                            LOG("no IP match -> no action \r\n");
                        }
                        break;

                    case CMD_SYNC_RESET:
                        LOG("CMD: Sync Reset: ");
                        sync_master_unset();
                        drift_timer_reset();
                        reset_drift_measure_params();
      
                        break;
                                            
                    default:
                        LOG("CMD: Unrecognized control command: %d\r\n", cmd);
                        break;
                }
            }
        }
    }
}

bool is_scanning_enabled(void){
    return m_scanning_enabled;
}

bool is_advertising_enabled(void){
    return m_advertising_enabled;
}

bool is_who_am_i_enabled(void){
    return m_who_am_i_enabled;
}

bool is_server_ip_received(void){
    return m_server_ip_received;
}

bool is_network_busy(void){
    return m_network_is_busy;
}

void set_network_busy(bool val){
   m_network_is_busy = val;
}

void get_target_ip_and_port(uint8_t* p_IP, uint32_t* p_port){
    memcpy(p_IP, target_IP, 4);
    *p_port = target_port;
}

void get_own_MAC(){

}