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
#include "boards.h"
#include "pwm.h"
#include "scan.h"
#include "nrf_gpio.h"
#include "util.h"
#include "dhcp.h"
#include "time_sync_timer.h"
#include "dhcp_cb.h"
#include "user_ethernet.h"
#include "timer_drift_measurement.h"
#include "ethernet_network.h"
#include "ppi.h"
#include "gpio.h"
#include "sync_timer_handler.h"


#ifdef MESH_ENABLED
#include "ethernet_dfu.h"
#include "time_sync_v1_controller.h"
#endif


static volatile float led_hp_default_value  = LED_HP_CONNECTED_DUTY_CYCLE;
static volatile uint32_t sync_interval      = SYNC_INTERVAL_MS;

static volatile bool m_controls_sync_signal   = false;
static volatile bool m_scanning_enabled       = true;
static volatile bool m_advertising_enabled    = false;
static volatile bool m_who_am_i_enabled       = false;


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
    sync_master_timer_init(interval);
    sync_master_gpio_init();
    sync_master_ppi_init();

    START_SYNC_TIMER = 1;
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

/* Function to send scan reports over Ethernet using UDP */
void send_scan_report(scan_report_t * scan_report)
{
    uint8_t buf[SCAN_REPORT_LENGTH];
    uint8_t len = 0;
    
    if(!is_network_busy())
    {
        set_network_busy(true);
        
        sprintf((char *)&buf[0], "{ \"nodeID\" : \"%02x:%02x:%02x:%02x:%02x:%02x\", \"timestamp\" : %d, \t \"counter\" : %d, \t \"address\" : \"%02x:%02x:%02x:%02x:%02x:%02x\", \"RSSI\" : %d, \"channel\" : %d, \"CRC\" : %01d, \"LPE\" : %01d, \"syncController\" : %01d }\r\n", 
                        scan_report->id[0], scan_report->id[1], scan_report->id[2], scan_report->id[3], scan_report->id[4], scan_report->id[5],
                        scan_report->timestamp, scan_report->counter, scan_report->address[0], scan_report->address[1], scan_report->address[2], 
                        scan_report->address[3], scan_report->address[4], scan_report->address[5],
                        scan_report->rssi, scan_report->channel, scan_report->crc_status, scan_report->long_packet_error, m_controls_sync_signal);
        
        len = strlen((const char *)&buf[0]);
        
        uint8_t target_IP[4] = {10, 0, 0, 4};    
        uint32_t target_port = 15000;
        get_target_IP_and_port(target_IP, &target_port);

        sendto(SOCKET_UDP, &buf[0], len, target_IP, target_port);

        
        set_network_busy(false);
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

        uint8_t own_IP[4] = {0};
        get_own_IP(own_IP);
        
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
                    case RESET_SYNC:
                        LOG("CMD: RESETING TIME SYNC\r\n");
                        sync_timer_reset();
                        break;
                    case WHOAMI_START:
                        LOG("CMD: WHO AM I start\r\n");
                        m_who_am_i_enabled = true;
                        break;

                    case WHOAMI_STOP:
                        LOG("CMD: WHO AM I stop\r\n");
                        m_who_am_i_enabled = false;
                        break;

                    case CMD_SERVER_IP_BROADCAST:
                        if (!is_server_IP_received())
                        {
                            get_server_ip(p_payload, payload_len);
                        }
                        set_server_IP_received(true);
                        break;

                    case CMD_NEW_SERVER_IP:
                        set_server_IP_received(false);
                        break;

                    #ifdef MESH_ENABLED

                    case CMD_NEW_FIRMWARE_ALL:
                        if(own_IP[0] != 1)
                        {
                          dfu_erase_flash_page();
                          dfu_write_own_ip(own_IP);
                          dfu_write_server_ip(&broadcast_ip[0]);
                          dfu_initiate_and_reset();
                        }
                        else
                        {
                          LOG("CMD: Own IP not valid - can not start DFU\r\n");
                        }
                        break;

                    case CMD_NEW_FIRMWARE_MAC:
                        if(own_IP[0] != 1)
                        {
                          uint8_t own_mac[6];
                          getSHAR(own_mac);

                          if(recv_len == 24)
                          {
                            if(own_mac[0] == received_data[18] && own_mac[1] == received_data[19] && own_mac[2] == received_data[20] && 
                            own_mac[3] == received_data[21] && own_mac[4] == received_data[22] && own_mac[5] == received_data[23])
                            {
                              dfu_erase_flash_page();
                              dfu_write_own_ip(own_IP);
                              dfu_write_server_ip(&broadcast_ip[0]);
                              dfu_initiate_and_reset();
                            }  
                          }  
                        }
                        else
                        {
                          LOG("CMD: Own IP not valid - can not start DFU\r\n");
                        }
                        break;

                    case CMD_NEW_FIRMWARE_BUTTON_ENABLE:
                        if(own_IP[0] != 1)
                        {
                          LOG("CMD: Set button DFU flag\r\n");
                          dfu_erase_flash_page();
                          dfu_write_own_ip(own_IP);
                          dfu_write_server_ip(&broadcast_ip[0]);
                          dfu_set_button_flag();
                        }
                        else
                        {
                          LOG("CMD: Own IP not valid - can not start DFU\r\n");
                        }             
                        break;

                    case CMD_NEW_FIRMWARE_BUTTON_DISABLE:
                        LOG("CMD: Clear button DFU flag\r\n");
                        dfu_clear_button_flag();
                        break;

                    #endif

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
                        LOG("CMD: Single HP LED ON: \n");

                        if (IPs_are_equal((uint8_t *)p_payload, own_IP))
                        {
                            pwm_set_duty_cycle(LED_HP, LED_HP_ON_DUTY_CYCLE);
                            LOG("IP match -> turning HP LED ON\r\n");
                            send_timestamp();
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
                        LOG("CMD: Sync node set: \n");
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
                        LOG("CMD: Sync Reset\n");
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
