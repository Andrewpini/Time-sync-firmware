/* Copyright (c) 2010 - 2018, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
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
 */

#include <stdint.h>
#include <string.h>

/* GPIO */
#include "boards.h"
#include "app_timer.h"

/* Core */
#include "nrf_mesh_config_core.h"
#include "nrf_mesh_gatt.h"
#include "nrf_mesh_configure.h"
#include "nrf_mesh.h"
#include "mesh_stack.h"
#include "device_state_manager.h"
#include "access_config.h"
#include "proxy.h"

/* Provisioning and configuration */
#include "mesh_provisionee.h"
#include "mesh_app_utils.h"

/* Models */
#include "rssi_server.h"
#include "rssi_util.h"
#include "rssi_common.h"
#include "health_client.h"
#include "time_sync_v1_controller.h"

/* Logging and RTT */
#include "log.h"
#include "rtt_input.h"

/* Example specific includes */
#include "app_config.h"
#include "example_common.h"
#include "nrf_mesh_config_examples.h"
#include "light_switch_example_common.h"
#include "ble_softdevice_support.h"
#include "example_network_config.h"

/* Ethernet DFU*/
#include "ethernet_dfu.h"

//NOTE: Trial and error for implementing the init routines from the ethernet node project
#include "gpio.h"
#include "command_system.h"
#include "pwm.h"
#include "ppi.h"
#include "clock.h"
#include "user_ethernet.h"
#include "user_spi.h"
#include "ethernet_network.h"
#include "command_system.h"
#include "timer_drift_measurement.h"
#include "time_sync_timer.h"
#include "config.h"
#include "socket.h"
#include "time_sync_v1_controller.h"
#include "sync_timer_handler.h"
#include "i_am_alive.h"


static const uint8_t appkey[16] = {0x71, 0x6F, 0x72, 0x64, 0x69, 0x63, 0x5F, 0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x5F, 0x31};
static dsm_handle_t m_netkey_handle;
static dsm_handle_t m_appkey_handle;

static rssi_server_t m_rssi_server;
static health_client_t m_health_client;
static rssi_util_t m_rssi_util;

/* Time sync v1 */
static uint8_t own_MAC[6] = {0};
static uint8_t mesh_node_gap_name[18];
static time_sync_controller_t m_time_sync_controller;
static dsm_handle_t m_time_sync_subscribe_handle;
static dsm_handle_t m_time_sync_publish_handle;

static dsm_handle_t health_subscribe_handle;
static dsm_handle_t health_publish_handle;
static dsm_handle_t rssi_util_subscribe_handle;
static dsm_handle_t rssi_util_publish_handle;
static dsm_handle_t rssi_server_publish_handle;

static bool m_device_provisioned;

static void app_health_event_cb(const health_client_t * p_client, const health_client_evt_t * p_event) 
{
    if(p_event->p_meta_data->p_core_metadata->source == NRF_MESH_RX_SOURCE_SCANNER)
    {
        dsm_local_unicast_address_t element_addr;
        rssi_util_mac_to_element_addr_find(&m_rssi_util, p_event->p_meta_data->p_core_metadata->params.scanner.adv_addr.addr, &element_addr);
        rssi_server_add_rssi_data(element_addr.address_start, p_event->p_meta_data->p_core_metadata->params.scanner.rssi);
    }
}

static void app_time_sync_event_cb(sync_event_t sync_event) 
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "src: %d\n", sync_event.sender.addr);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "dst: %d\n", sync_event.reciver.addr);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "src timestamp: %d\n", sync_event.sender.timestamp);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "dst timestamp: %d\n", sync_event.reciver.timestamp);
}

static void app_rssi_server_cb(const rssi_data_entry_t* p_data, uint8_t length) // TODO: Seems like packets almost only are sent one way?
{
        uint8_t buf[SCAN_REPORT_LENGTH];
        uint8_t len = 0;

        dsm_local_unicast_address_t local_addr;
        dsm_local_unicast_addresses_get(&local_addr);


        #ifdef BROADCAST_ENABLED
            uint8_t target_IP[4] = {255, 255, 255, 255}; 
            uint32_t target_port = 11035;;
        #else
            uint8_t target_IP[4];
            get_target_IP(target_IP);       
            uint32_t target_port = 11035;
        #endif

        buf[0] = (uint8_t)((local_addr.address_start & 0xFF00) >> 8);
        buf[1] = (uint8_t)(local_addr.address_start & 0x00FF);

        uint8_t i;

        for(i=0; i<length; i++)
        {
          buf[2] = (uint8_t)(((p_data+i)->src_addr & 0xFF00) >> 8);
          buf[3] = (uint8_t)((p_data+i)->src_addr & 0x00FF);
          buf[4] = (p_data+i)->mean_rssi;
          buf[5] = (p_data+i)->msg_count;

          len = 6;
             
          int32_t err = sendto(SOCKET_UDP, &buf[0], len, target_IP, target_port);

          if(err < 0)
          {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Error sending packet (app_rssi_server_cb): %d\n", err);
          }
        }
}

/*************************************************************************************************/

static void config_server_evt_cb(const config_server_evt_t * p_evt)
{
    if (p_evt->type == CONFIG_SERVER_EVT_NODE_RESET){
        led_blink_ms(LED_BLINK_INTERVAL_MS, LED_BLINK_CNT_RESET);
        mesh_stack_device_reset();
    }
}

static void device_identification_start_cb(uint8_t attention_duration_s)
{
    led_blink_ms(LED_BLINK_ATTENTION_INTERVAL_MS, LED_BLINK_ATTENTION_COUNT(attention_duration_s));
}

static void provisioning_aborted_cb(void)
{
     led_blink_stop();
}

static void provisioning_complete_cb(void)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Successfully provisioned\n");

#if MESH_FEATURE_GATT_ENABLED
    /* Restores the application parameters after switching from the Provisioning
     * service to the Proxy  */
    gap_params_init(mesh_node_gap_name);
    conn_params_init();
#endif

    dsm_local_unicast_address_t node_address;
    dsm_local_unicast_addresses_get(&node_address);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Node Address: 0x%04x \n", node_address.address_start);

    led_blink_stop();
    led_blink_ms(LED_BLINK_INTERVAL_MS, LED_BLINK_CNT_PROV);

    m_netkey_handle = dsm_net_key_index_to_subnet_handle(0);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "NETKEYHANDLE: %d\n", m_netkey_handle);

    ERROR_CHECK(dsm_appkey_add(0, m_netkey_handle, appkey, &m_appkey_handle));

    health_server_t* p_health_server;
    p_health_server = mesh_stack_health_server_get();

    ERROR_CHECK(dsm_address_subscription_add(HEALTH_GROUP_ADDRESS, &health_subscribe_handle));
    ERROR_CHECK(dsm_address_publish_add(HEALTH_GROUP_ADDRESS, &health_publish_handle));
    ERROR_CHECK(dsm_address_subscription_add(RSSI_UTIL_GROUP_ADDRESS, &rssi_util_subscribe_handle));
    ERROR_CHECK(dsm_address_publish_add(RSSI_UTIL_GROUP_ADDRESS, &rssi_util_publish_handle));
    ERROR_CHECK(dsm_address_publish_add(PROVISIONER_ADDRESS, &rssi_server_publish_handle));

    ERROR_CHECK(access_model_application_bind(p_health_server->model_handle, m_appkey_handle));
    ERROR_CHECK(access_model_publish_application_set(p_health_server->model_handle, m_appkey_handle));
    ERROR_CHECK(access_model_publish_address_set(p_health_server->model_handle, health_publish_handle));
    ERROR_CHECK(access_model_publish_period_set(p_health_server->model_handle, ACCESS_PUBLISH_RESOLUTION_1S, 1));

    /* Bind health client to App key, and configure publication key */
    ERROR_CHECK(access_model_application_bind(m_health_client.model_handle, m_appkey_handle));
    ERROR_CHECK(access_model_publish_application_set(m_health_client.model_handle, m_appkey_handle));
    ERROR_CHECK(access_model_subscription_list_alloc(m_health_client.model_handle));
    ERROR_CHECK(access_model_subscription_add(m_health_client.model_handle, health_subscribe_handle));

    ERROR_CHECK(access_model_application_bind(m_rssi_util.model_handle, m_appkey_handle));
    ERROR_CHECK(access_model_publish_application_set(m_rssi_util.model_handle, m_appkey_handle));
    ERROR_CHECK(access_model_subscription_list_alloc(m_rssi_util.model_handle));
    ERROR_CHECK(access_model_subscription_add(m_rssi_util.model_handle, rssi_util_subscribe_handle));
    ERROR_CHECK(access_model_publish_address_set(m_rssi_util.model_handle, rssi_util_publish_handle));

    ERROR_CHECK(access_model_application_bind(m_rssi_server.model_handle, m_appkey_handle));
    ERROR_CHECK(access_model_publish_application_set(m_rssi_server.model_handle, m_appkey_handle));
    ERROR_CHECK(access_model_publish_address_set(m_rssi_server.model_handle, rssi_server_publish_handle));
    ERROR_CHECK(access_model_publish_period_set(m_rssi_server.model_handle, ACCESS_PUBLISH_RESOLUTION_1S, 10));

    /*Time sync model*/
    ERROR_CHECK(dsm_address_subscription_add(TIME_SYNC_GROUP_ADDRESS, &m_time_sync_subscribe_handle));
    ERROR_CHECK(dsm_address_publish_add(TIME_SYNC_GROUP_ADDRESS, &m_time_sync_publish_handle));

    ERROR_CHECK(access_model_application_bind(m_time_sync_controller.model_handle, m_appkey_handle));
    ERROR_CHECK(access_model_publish_application_set(m_time_sync_controller.model_handle, m_appkey_handle));
    ERROR_CHECK(access_model_subscription_add(m_time_sync_controller.model_handle, rssi_util_subscribe_handle));
    ERROR_CHECK(access_model_publish_address_set(m_time_sync_controller.model_handle, rssi_util_publish_handle));
    ERROR_CHECK(access_model_publish_period_set(m_time_sync_controller.model_handle, ACCESS_PUBLISH_RESOLUTION_1S, 1));

    access_flash_config_store();
}


static void models_init_cb(void)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Initializing and adding models\n");
    ERROR_CHECK(rssi_server_init(&m_rssi_server, 0, app_rssi_server_cb));

    ERROR_CHECK(rssi_util_init(&m_rssi_util));
    ERROR_CHECK(access_model_subscription_list_alloc(m_rssi_util.model_handle));

    ERROR_CHECK(health_client_init(&m_health_client, 0, app_health_event_cb));
    ERROR_CHECK(access_model_subscription_list_alloc(m_health_client.model_handle));

    ERROR_CHECK(time_sync_controller_init(&m_time_sync_controller, 0, app_time_sync_event_cb));
    ERROR_CHECK(access_model_subscription_list_alloc(m_time_sync_controller.model_handle));
}


static void initialize(uint8_t * gap_name)
{
    /* Make sure that high power LED is disabled */
    nrf_gpio_cfg_output(13);
    nrf_gpio_pin_clear(13);

    ble_stack_init();

    #if MESH_FEATURE_GATT_ENABLED
        gap_params_init(gap_name);
        conn_params_init();
    #endif

    mesh_stack_init_params_t init_params =
    {
        .core.irq_priority       = NRF_MESH_IRQ_PRIORITY_LOWEST,
        .core.lfclksrc           = DEV_BOARD_LF_CLK_CFG,
        .core.p_uuid             = NULL,
        .models.models_init_cb   = models_init_cb,
        .models.config_server_cb = config_server_evt_cb 
    };
    ERROR_CHECK(mesh_stack_init(&init_params, &m_device_provisioned));

    if (!m_device_provisioned)
    {
        static const uint8_t static_auth_data[NRF_MESH_KEY_SIZE] = STATIC_AUTH_DATA;
        mesh_provisionee_start_params_t prov_start_params =
        {
            .p_static_data    = static_auth_data,
            .prov_complete_cb = provisioning_complete_cb,
            .prov_device_identification_start_cb = device_identification_start_cb,
            .prov_device_identification_stop_cb = NULL,
            .prov_abort_cb = provisioning_aborted_cb,
            .p_device_uri = EX_URI_LS_SERVER
        };
        ERROR_CHECK(mesh_provisionee_prov_start(&prov_start_params));
    }
    ERROR_CHECK(mesh_stack_start());

    mesh_app_uuid_print(nrf_mesh_configure_device_uuid_get());
    led_blink_ms(LED_BLINK_INTERVAL_MS, LED_BLINK_CNT_START);
}

static void app_rtt_input_handler(int key)
{
    switch(key)
    {
        case '0':
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "SENDING TIMESTAMP\n");
            send_timestamp();
            break;

        case '1':
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "PUB_TIMER ON\n");
            sync_set_pub_timer(true);
            break;

        case '2':
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "PUB_TIMER OFF\n");
            sync_set_pub_timer(false);
            break;

        case '3':
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "SET TIMER OFFSET TO 2000 \n");
            sync_timer_set_timer_offset(2000);
            break;

        default:
            break;
    }
}

int main(void)
{
    clock_init();
    __LOG_INIT(LOG_SRC_APP | LOG_SRC_ACCESS | LOG_SRC_BEARER, LOG_LEVEL_INFO, LOG_CALLBACK_DEFAULT);
    ERROR_CHECK(app_timer_init());

    leds_init();
    button_init_dfu();
    sync_line_init();
    drift_timer_init();
    gpiote_init();
    ppi_init();
    spi0_master_init();
    user_ethernet_init();
    dhcp_init();
    broadcast_init();
    connection_init();
    
    #ifdef BROADCAST_ENABLED
        /* Hardcoded 'true' since we are using broadcast*/
        set_server_IP_received(true);

    #else
        while(!is_server_IP_received())
        {
            check_ctrl_cmd();
        }
    #endif

    pwm_set_duty_cycle(LED_HP, LED_HP_DEFAULT_DUTY_CYCLE);

    rtt_input_enable(app_rtt_input_handler, RTT_INPUT_POLL_PERIOD_MS);

    get_own_MAC(own_MAC);
    sprintf((char *)&mesh_node_gap_name[0], "%02x:%02x:%02x:%02x:%02x:%02x", own_MAC[0], own_MAC[1], own_MAC[2], own_MAC[3], own_MAC[4], own_MAC[5]);

    initialize(mesh_node_gap_name);
    ERROR_CHECK(dfu_clear_bootloader_flag());
    i_am_alive_timer_init();

    while(1){
       if (is_connected())
              {
                  if (is_server_IP_received())
                  {
                      send_drift_timing_sample();
                  }
                  check_ctrl_cmd();
              }
        (void)sd_app_evt_wait();
    }
}
