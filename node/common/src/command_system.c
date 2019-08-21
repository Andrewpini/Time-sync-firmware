#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "command_system.h"
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "w5500.h"
#include "socket.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "boards.h"
#include "pwm.h"
#include "nrf_gpio.h"
#include "util.h"
#include "utils.h" // Mesh core utils
#include "dhcp.h"
#include "time_sync_timer.h"
#include "dhcp_cb.h"
#include "ethernet.h"
#include "sync_line.h"
#include "ppi.h"
#include "gpio.h"
#include "sync_timer_handler.h"
#include "ethernet_dfu.h"
#include "time_sync_controller.h"

/* Function for checking if the device has received a new control command */
void check_ctrl_cmd(void)
{
    uint8_t received_data[200];
    command_system_package_t received_package;

    uint8_t own_mac[6];
    get_own_MAC(own_mac);
    uint8_t own_ip[4];
    uint8_t uninitialized_ip[4] = {1, 1, 1, 1};

    uint8_t received_from_ip[4] = {0};
    uint16_t broadcast_port = BROADCAST_PORT;

    while (getSn_RX_RSR(SOCKET_RX) != 0x00)
    {
        get_own_IP(own_ip);
        
        /* Receive new data from socket */
        int32_t recv_len = recvfrom(SOCKET_RX, received_data, sizeof(received_data), received_from_ip, &broadcast_port);

        /* If any data is received, check which command it is */
        if (recv_len >= 4) /* 4 because of the first memcpy */
        {
            uint32_t identifier;
            memcpy(&identifier, received_data, 4);
//            uint32_t identifier = LE2BE32(reversed_identifier);

            if (identifier == 0xDEADFACE)
            {
                memcpy(&received_package, received_data, sizeof(received_package));

                /* Choose the right action according to command */
                switch (received_package.opcode)
                {
                    case CMD_RESET_ALL_NODES:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Reset all nodes\n");
                        ERROR_CHECK(sd_nvic_SystemReset());
                        break;
            
                    case CMD_RESET_NODE_MAC:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Reset node - MAC\n");
                        if (mac_addresses_are_equal(own_mac, received_package.mac))
                        {
                            ERROR_CHECK(sd_nvic_SystemReset());
                        } 
                        else
                        {
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "No MAC match -> no action\n");
                        }
                        break;

                    case CMD_DFU_ALL:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: DFU all\n");
                                                
                        if (!ip_addresses_are_equal(own_ip, uninitialized_ip))
                        {
                            dfu_erase_flash_page();
                            dfu_write_own_ip(own_ip);
                            dfu_write_server_ip(received_from_ip);
                            dfu_initiate_and_reset();
                        }
                        else
                        {
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Own IP not valid - can not start DFU\r\n");
                        }
                        break;

                    case CMD_DFU_MAC:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: DFU - MAC\n");

                        if (!ip_addresses_are_equal(own_ip, uninitialized_ip))
                        {
                            if(mac_addresses_are_equal(own_mac, received_package.mac))
                            {
                              dfu_erase_flash_page();
                              dfu_write_own_ip(own_ip);
                              dfu_write_server_ip(received_from_ip);
                              dfu_initiate_and_reset();
                            }  
                        }
                        else
                        {
                          __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Own IP not valid - can not start DFU\r\n");
                        }
                        break;

                    case CMD_DFU_BUTTON_ENABLE:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: DFU button enable\n");
                        if(!ip_addresses_are_equal(own_ip, uninitialized_ip))
                        {
                          dfu_erase_flash_page();
                          dfu_write_own_ip(own_ip);
                          dfu_write_server_ip(received_from_ip);
                          dfu_set_button_flag();
                        }
                        else
                        {
                          __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Own IP not valid - can not start DFU\n");
                        }             
                        break;

                    case CMD_DFU_BUTTON_DISABLE:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: DFU button disable\n");
                        dfu_clear_button_flag();
                        break;

                    case CMD_ALL_HPLED_ON:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: All HP LED - ON\n");
                        pwm_set_duty_cycle(LED_HP, LED_HP_ON_DUTY_CYCLE);
                        break;

                    case CMD_ALL_HPLED_OFF:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: All HP LED - OFF\n");
                        pwm_set_duty_cycle(LED_HP, LED_HP_OFF_DUTY_CYCLE);
                        break;

                    case CMD_SINGLE_HPLED_ON:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Single HP LED - ON\n");

                        if (mac_addresses_are_equal(own_mac, received_package.payload.hp_led_package.target_mac))
                        {
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "MAC match -> turning HP LED ON\n");
                            pwm_set_duty_cycle(LED_HP, LED_HP_ON_DUTY_CYCLE);
                            
                            ack_package_t ack_package;
                            get_own_MAC((uint8_t*)ack_package.mac);
                            ack_package.tid = received_package.payload.hp_led_package.tid;
                            send_over_ethernet((uint8_t*)&ack_package, CMD_ACK);
                        }
                        else 
                        {
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "No MAC match -> no action\n");
                        }
                        break;

                    case CMD_SINGLE_HPLED_OFF:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Single HP LED - OFF\n");

                        if (mac_addresses_are_equal(own_mac, received_package.payload.hp_led_package.target_mac))
                        {
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "MAC match -> turning HP LED OFF\n");
                            pwm_set_duty_cycle(LED_HP, LED_HP_OFF_DUTY_CYCLE);

                            ack_package_t ack_package;
                            get_own_MAC((uint8_t*)ack_package.mac);
                            ack_package.tid = received_package.payload.hp_led_package.tid;
                            send_over_ethernet((uint8_t*)&ack_package, CMD_ACK);
                        }
                        else 
                        {
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "No MAC match -> no action\n");
                        }
                        break;

                     case CMD_SYNC_LINE_START_MASTER:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Sync node set: \n");

                        if (mac_addresses_are_equal(own_mac, received_package.payload.hp_led_package.target_mac))
                        {
                            sync_master_set(SYNC_INTERVAL_MS);
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "MAC match -> setting node as sync master \r\n");

                            ack_package_t ack_package;
                            get_own_MAC((uint8_t*)ack_package.mac);
                            ack_package.tid = received_package.payload.hp_led_package.tid;
                            ack_package.ack_opcode = CMD_SYNC_LINE_START_MASTER;
                            send_over_ethernet((uint8_t*)&ack_package, CMD_ACK);
                        }
                        else 
                        {
                            sync_master_unset();
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "no MAC match -> unset as sync line master, no further action \r\n");
                        }
                        break;

                    case CMD_SYNC_LINE_RESET:{
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Sync Reset\n");
                        sync_master_unset();
                        drift_timer_reset();
                        reset_drift_measure_params();

                        ack_package_t ack_package;
                        get_own_MAC((uint8_t*)ack_package.mac);
                        ack_package.tid = received_package.payload.hp_led_package.tid;
                        ack_package.ack_opcode = CMD_SYNC_LINE_RESET;
                        send_over_ethernet((uint8_t*)&ack_package, CMD_ACK);
                        break;
                    }

                    case CMD_SYNC_LINE_STOP:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Sync line stop\n");
                        break;

                    case CMD_TIME_SYNC_START_MASTER:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Time sync start master\n");
                        if (mac_addresses_are_equal(own_mac, received_package.payload.hp_led_package.target_mac))
                        {
                            sync_set_pub_timer(true);
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "MAC match -> setting node as time sync master \r\n");
                            ack_package_t ack_package;
                            get_own_MAC((uint8_t*)ack_package.mac);
                            ack_package.tid = received_package.payload.hp_led_package.tid;
                            ack_package.ack_opcode = CMD_TIME_SYNC_START_MASTER;
                            send_over_ethernet((uint8_t*)&ack_package, CMD_ACK);
                        }
                        else 
                        {
                            sync_set_pub_timer(false);
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "No MAC match -> unset as time sync master, no further action \r\n");
                        }
                        break;

                    case CMD_TIME_SYNC_STOP:{
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Time sync stop\n");
                        sync_set_pub_timer(false);
                        ack_package_t ack_package;
                        get_own_MAC((uint8_t*)ack_package.mac);
                        ack_package.tid = received_package.payload.hp_led_package.tid;
                        ack_package.ack_opcode = CMD_TIME_SYNC_STOP;
                        send_over_ethernet((uint8_t*)&ack_package, CMD_ACK);
                        break;
                    }
                                    
                    default:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Unrecognized control command: %d\r\n", received_package.opcode);
                        break;
                }
            }
        }
    }
}
