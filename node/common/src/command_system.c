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
#include "ethernet_utils.h"
#include "utils.h"
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
#include "mesh_opt_core.h"
#include "radio_config.h"
#include "core_tx.h"

static uint8_t received_data[200];
static command_system_package_t received_package;

static uint8_t own_mac[6];
static uint8_t own_ip[4];
static uint8_t uninitialized_ip[4] = {1, 1, 1, 1};

static uint8_t server_ip[4] = {1, 1, 1, 1};
static uint16_t broadcast_port = COMMAND_RX_PORT;

/* Function for checking if the device has received a new control command */
void check_ctrl_cmd(void)
{
    while (getSn_RX_RSR(SOCKET_RX) != 0x00)
    {
        get_own_ip(own_ip);
        
        /* Receive new data from socket */
        int32_t recv_len = recvfrom(SOCKET_RX, received_data, sizeof(received_data), server_ip, &broadcast_port);

        /* If any data is received, check which command it is */
        if (recv_len >= 4) /* 4 because of the first memcpy */
        {
            uint32_t identifier;
            memcpy(&identifier, received_data, 4);

            if (identifier == 0xDEADFACE)
            {
                memcpy(&received_package, received_data, sizeof(received_package));

                /* Choose the right action according to command */
                switch (received_package.opcode)
                {
                    case CMD_TX_POWER:
                        if(received_package.payload.tx_power_package.is_broadcast || mac_addresses_are_equal(own_mac, received_package.payload.tx_power_package.target_mac))
                        {   
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Radio idx: %d\n", received_package.payload.tx_power_package.selected_pwr_idx);
                            radio_tx_power_t radio_pwr;
                            mesh_opt_core_tx_power_get(CORE_TX_ROLE_RELAY, &radio_pwr);
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Radio Before Power is: %d\n",radio_pwr);
                            ERROR_CHECK(mesh_opt_core_tx_power_set(CORE_TX_ROLE_RELAY, tx_power_array[received_package.payload.tx_power_package.selected_pwr_idx]));
                            mesh_opt_core_tx_power_get(CORE_TX_ROLE_RELAY, &radio_pwr);
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Radio After Power is: %d\n",radio_pwr);
                            ERROR_CHECK(mesh_opt_core_tx_power_set(CORE_TX_ROLE_ORIGINATOR, tx_power_array[received_package.payload.tx_power_package.selected_pwr_idx]));
                        }
                        break;

                    case CMD_RESET:
                        if(received_package.payload.reset_package.is_broadcast || mac_addresses_are_equal(own_mac, received_package.payload.reset_package.target_mac))
                        {    
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Reset node\n");
                            ERROR_CHECK(sd_nvic_SystemReset());
                        }
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

                    case CMD_DFU:
                        if(received_package.payload.dfu_package.is_broadcast || mac_addresses_are_equal(own_mac, received_package.payload.dfu_package.target_mac))
                        {    
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: DFU\n");                
                            if (ip_addresses_are_equal(own_ip, uninitialized_ip))
                            {
                                __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Own IP not valid - can not start DFU\r\n");
                                break;
                            }
                            dfu_start();
                        }  
                        break;

                    case CMD_LED:
                        if (received_package.payload.led_package.is_broadcast || mac_addresses_are_equal(own_mac, received_package.payload.led_package.target_mac))
                        {
                            if(received_package.payload.led_package.on_off)
                            {
                                __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Recieved HP-LED-ON command\n");
                                pwm_set_duty_cycle(LED_HP, LED_HP_ON_DUTY_CYCLE);
                            }
                            else
                            {
                                __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Recieved HP-LED-OFF command\n");
                                pwm_set_duty_cycle(LED_HP, LED_HP_OFF_DUTY_CYCLE);
                            }
                        }
                        break;


                     case CMD_SYNC_LINE_START_MASTER:
                        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "CMD: Sync node set: \n");

                        if (mac_addresses_are_equal(own_mac, received_package.payload.hp_led_package.target_mac))
                        {
                            sync_master_set(SYNC_LINE_INTERVAL_MS);
                            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "MAC match -> setting node as sync master \r\n");

                            ack_package_t ack_package;
                            get_own_mac((uint8_t*)ack_package.mac);
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
                        get_own_mac((uint8_t*)ack_package.mac);
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
                            get_own_mac((uint8_t*)ack_package.mac);
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
                        get_own_mac((uint8_t*)ack_package.mac);
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

void command_system_set_mac(void)
{
    get_own_mac(own_mac);
}

void get_server_ip(uint8_t* p_server_ip)
{
    memcpy(p_server_ip, server_ip, 4);
}
