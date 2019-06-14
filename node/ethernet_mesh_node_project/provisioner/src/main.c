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

/* HAL */
#include "boards.h"
#include "nrf_delay.h"
#include "simple_hal.h"
#include "app_timer.h"

/* Core */
#include "nrf_mesh.h"
#include "nrf_mesh_events.h"
#include "nrf_mesh_assert.h"
#include "access_config.h"
#include "device_state_manager.h"
#include "flash_manager.h"
#include "mesh_stack.h"
#include "net_state.h"

/* Provisioning and configuration */
#include "provisioner_helper.h"
#include "node_setup.h"
#include "mesh_app_utils.h"

/* Models */
#include "config_client.h"
#include "config_server.h"
#include "health_client.h"
#include "health_server.h"
#include "rssi_util.h"
#include "rssi_server.h"
#include "rssi_client.h"

/* Logging and RTT */
#include "rtt_input.h"
#include "log.h"

/* Example specific includes */
#include "light_switch_example_common.h"
#include "example_network_config.h"
#include "nrf_mesh_config_examples.h"
#include "ble_softdevice_support.h"
#include "example_common.h"
#include "config_and_prov.h"
#include "nrf_mesh_serial.h"

#define APP_NETWORK_STATE_ENTRY_HANDLE (0x0001)
#define APP_FLASH_PAGE_COUNT           (1)

#define APP_PROVISIONING_LED            BSP_LED_0
#define APP_CONFIGURATION_LED           BSP_LED_1

/* Required for the provisioner helper module */
static network_dsm_handles_data_volatile_t m_dev_handles;
static network_stats_data_stored_t m_nw_state;

static rssi_util_t m_rssi_util;
static rssi_server_t m_rssi_server;
static rssi_client_t m_rssi_client;
static dsm_handle_t health_subscribe_handle;
static dsm_handle_t health_publish_handle;
static dsm_handle_t rssi_util_subscribe_handle;
static dsm_handle_t rssi_util_publish_handle;
static dsm_handle_t rssi_server_publish_handle;

static uint16_t config_address_pointer = 0x100;
static uint16_t prov_address_pointer = 0x100;
static uint16_t current_reset_addr;

/* Forward declarations */
static void app_health_event_cb(const health_client_t * p_client, const health_client_evt_t * p_event);
static void app_config_successful_cb(void);
static void app_config_failed_cb(void);
static void app_mesh_core_event_cb (const nrf_mesh_evt_t * p_evt);

static void app_start(void);

static nrf_mesh_evt_handler_t m_mesh_core_event_handler = { .evt_cb = app_mesh_core_event_cb };

/*****************************************************************************/
/**** Flash handling ****/
#if PERSISTENT_STORAGE

static flash_manager_t m_flash_manager;

static void app_flash_manager_add(void);

static void flash_write_complete(const flash_manager_t * p_manager, const fm_entry_t * p_entry, fm_result_t result)
{
     __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Flash write complete\n");

    /* If we get an AREA_FULL then our calculations for flash space required are buggy. */
    NRF_MESH_ASSERT(result != FM_RESULT_ERROR_AREA_FULL);

    /* We do not invalidate in this module, so a NOT_FOUND should not be received. */
    NRF_MESH_ASSERT(result != FM_RESULT_ERROR_NOT_FOUND);
    if (result == FM_RESULT_ERROR_FLASH_MALFUNCTION)
    {
        ERROR_CHECK(NRF_ERROR_NO_MEM);
    }
}

static void flash_invalidate_complete(const flash_manager_t * p_manager, fm_handle_t handle, fm_result_t result)
{
    /* This application does not expect invalidate complete calls. */
    ERROR_CHECK(NRF_ERROR_INTERNAL);
}

typedef void (*flash_op_func_t) (void);
static void flash_manager_mem_available(void * p_args)
{
    ((flash_op_func_t) p_args)(); /*lint !e611 Suspicious cast */
}


static void flash_remove_complete(const flash_manager_t * p_manager)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Flash remove complete\n");
}

static void app_flash_manager_add(void)
{

    static fm_mem_listener_t flash_add_mem_available_struct = {
        .callback = flash_manager_mem_available,
        .p_args = app_flash_manager_add
    };

    const uint32_t * start_address;
    uint32_t allocated_area_size;
    ERROR_CHECK(mesh_stack_persistence_flash_usage(&start_address, &allocated_area_size));

    flash_manager_config_t manager_config;
    manager_config.write_complete_cb = flash_write_complete;
    manager_config.invalidate_complete_cb = flash_invalidate_complete;
    manager_config.remove_complete_cb = flash_remove_complete;
    manager_config.min_available_space = WORD_SIZE;
    manager_config.p_area = (const flash_manager_page_t *)((uint32_t)start_address - PAGE_SIZE * APP_FLASH_PAGE_COUNT);
    manager_config.page_count = APP_FLASH_PAGE_COUNT;
    uint32_t status = flash_manager_add(&m_flash_manager, &manager_config);
    if (NRF_SUCCESS != status)
    {
        flash_manager_mem_listener_register(&flash_add_mem_available_struct);
        __LOG(LOG_SRC_APP, LOG_LEVEL_ERROR, "Unable to add flash manager for app data\n");
    }
}

static bool load_app_data(void)
{
    uint32_t length = sizeof(m_nw_state);
    uint32_t status = flash_manager_entry_read(&m_flash_manager,
                                               APP_NETWORK_STATE_ENTRY_HANDLE,
                                               &m_nw_state,
                                               &length);
    if (status != NRF_SUCCESS)
    {
        memset(&m_nw_state, 0x00, sizeof(m_nw_state));
    }

    return (status == NRF_SUCCESS);
}

static uint32_t store_app_data(void)
{
    fm_entry_t * p_entry = flash_manager_entry_alloc(&m_flash_manager, APP_NETWORK_STATE_ENTRY_HANDLE, sizeof(m_nw_state));
    static fm_mem_listener_t flash_add_mem_available_struct = {
        .callback = flash_manager_mem_available,
        .p_args = store_app_data
    };

    if (p_entry == NULL)
    {
        flash_manager_mem_listener_register(&flash_add_mem_available_struct);
    }
    else
    {
        network_stats_data_stored_t * p_nw_state = (network_stats_data_stored_t *) p_entry->data;
        memcpy(p_nw_state, &m_nw_state, sizeof(m_nw_state));
        flash_manager_entry_commit(p_entry);
    }

    return NRF_SUCCESS;
}

static void clear_app_data(void)
{
    memset(&m_nw_state, 0x00, sizeof(m_nw_state));

    if (flash_manager_remove(&m_flash_manager) != NRF_SUCCESS)
    {
        /* Register the listener and wait for some memory to be freed up before we retry. */
        static fm_mem_listener_t mem_listener = {.callback = flash_manager_mem_available,
                                                 .p_args = clear_app_data};
        flash_manager_mem_listener_register(&mem_listener);
    }
}

#else

static void clear_app_data(void)
{
    return;
}

bool load_app_data(void)
{
    return false;
}
static uint32_t store_app_data(void)
{
    return NRF_SUCCESS;
}

#endif


static void app_data_store_cb(void)
{
    ERROR_CHECK(store_app_data());
}

static const char * server_uri_index_select(const char * p_client_uri)
{
    if (strcmp(p_client_uri, EX_URI_LS_CLIENT) == 0 || strcmp(p_client_uri, EX_URI_ENOCEAN) == 0)
    {
        return EX_URI_LS_SERVER;
    }
    else if (strcmp(p_client_uri, EX_URI_DM_CLIENT) == 0)
    {
        return EX_URI_DM_SERVER;
    }

    __LOG(LOG_SRC_APP, LOG_LEVEL_ASSERT, "Invalid client URI index.\n");
    NRF_MESH_ASSERT(false);

    return NULL;
}

/*****************************************************************************/
/**** Configuration process related callbacks ****/

static void app_config_successful_cb(void)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Configuration of device %u successful\n", m_nw_state.configured_devices);

    raise_flag(config_address_pointer, CONFIG_FLAG, true);
    m_nw_state.configured_devices++;
    access_flash_config_store();
    ERROR_CHECK(store_app_data());
    clear_busy_flag();
}

static void app_config_failed_cb(void)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Configuration of device %u failed. Press Button 1 to retry.\n", m_nw_state.configured_devices);
    clear_busy_flag();
}

static void app_prov_success_cb(uint16_t element)
{
    add_entry(element);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Provisioning successful\n");
    clear_busy_flag();

    ERROR_CHECK(store_app_data());
}

static void app_prov_failed_cb(uint16_t element)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Provisioning failed. Press Button 1 to retry.\n");
    clear_busy_flag();

    hal_led_pin_set(APP_PROVISIONING_LED, 0);
}


/*****************************************************************************/
/**** Model related callbacks ****/
static void app_health_rssi_cb(const access_message_rx_t* p_message)
{
    uint8_t uart_link_array[128];
    uint8_t uart_link_array_length;
    rssi_data_entry_t link_info[32];

    memset(uart_link_array, NULL, sizeof(uart_link_array));
    memset(link_info, NULL, sizeof(link_info));

    uint8_t entry_counter = p_message->length / sizeof(rssi_data_entry_t);

    uart_link_array_length = (entry_counter * 4) + 4;
   
    uart_link_array[0] = 0x01;

    uint8_t for_loop_counter = 0;

    memcpy(link_info, p_message->p_data, p_message->length);

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "\n");
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "To: %x. Entry counter: %d\n", p_message->meta_data.src.value, entry_counter);

    // uart_link_array[1] = m_nw_state.configured_devices;
    uart_link_array[1] = 0xFF;
    uart_link_array[2] = (uint8_t)((p_message->meta_data.src.value & 0xFF00) >> 8);
    uart_link_array[3] = (uint8_t)(p_message->meta_data.src.value & 0x00FF);

    for(uint8_t i = 0; i < entry_counter; i++)
    {
        uart_link_array[for_loop_counter + 4] = (uint8_t)((link_info[i].src_addr & 0xFF00) >> 8);
        uart_link_array[for_loop_counter + 5] = (uint8_t)(link_info[i].src_addr & 0x00FF);
        uart_link_array[for_loop_counter + 6] = link_info[i].mean_rssi;
        uart_link_array[for_loop_counter + 7] = link_info[i].msg_count;

        for_loop_counter = for_loop_counter + 4;
        
        //__LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "(From: %x, RSSI: %d, Message count: %d)\n",link_info[i].src_addr, link_info[i].mean_rssi, link_info[i].msg_count);
    }

    uint32_t serial_error_code = nrf_mesh_serial_tx(uart_link_array, uart_link_array_length);
            
    if(serial_error_code)
    {
      __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "ERROR SENDING HEALTH DATA OVER UART. Error code: %d\n", serial_error_code);
    }else
    {
      //__LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "DATA SENT OVER UART\n");
    }
}

static void app_health_event_cb(const health_client_t * p_client, const health_client_evt_t * p_event)
{
    if(p_event->p_meta_data->p_core_metadata->source == NRF_MESH_RX_SOURCE_SCANNER)
    {
        dsm_local_unicast_address_t element_addr;
        rssi_util_mac_to_element_addr_find(&m_rssi_util, p_event->p_meta_data->p_core_metadata->params.scanner.adv_addr.addr, &element_addr);
        rssi_server_add_rssi_data(element_addr.address_start, p_event->p_meta_data->p_core_metadata->params.scanner.rssi);
    }
}

static void app_config_server_event_cb(const config_server_evt_t * p_evt)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "config_server Event %d.\n", p_evt->type);

    if (p_evt->type == CONFIG_SERVER_EVT_NODE_RESET)
    {
        /* This should never return */
        hal_device_reset(0);
    }
}

static void app_config_client_event_cb(config_client_event_type_t event_type, const config_client_event_t * p_event, uint16_t length)
{
    /* USER_NOTE: Do additional processing of config client events here if required */
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Config client event\n");

    if(p_event->opcode == CONFIG_OPCODE_NODE_RESET_STATUS)
    {
            dsm_handle_t devkey_handle;
            dsm_devkey_handle_get(current_reset_addr, &devkey_handle);
            dsm_devkey_delete(devkey_handle);
            config_client_server_publish_addr_reset();

            dsm_handle_t address_handle;
            const nrf_mesh_address_t mesh_address = {.type = NRF_MESH_ADDRESS_TYPE_UNICAST, .value = current_reset_addr};
            dsm_address_handle_get(&mesh_address, &address_handle);
            dsm_address_publish_remove(address_handle);

            if (dsm_address_handle_get(&mesh_address, &address_handle) == NRF_SUCCESS)
            {
              __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Deleting the address didn't actually remove it...\n");
            }
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Removing devkey handle for addr: 0x%x\n", current_reset_addr);

            delete_entry(current_reset_addr);
            clear_busy_flag();
    }
    else
    {
        /* Pass events to the node setup helper module for further processing */
        node_setup_config_client_event_process(event_type, p_event, length);
    }
}

static void app_mesh_core_event_cb (const nrf_mesh_evt_t * p_evt)
{
    /* USER_NOTE: User can insert mesh core event proceesing here */
    switch(p_evt->type)
    {
        /* Start user application specific functions only when flash is stable */
        case NRF_MESH_EVT_FLASH_STABLE:
            __LOG(LOG_SRC_APP, LOG_LEVEL_DBG1, "Mesh evt: FLASH_STABLE \n");
#if (PERSISTENT_STORAGE)
            {
                static bool s_app_started;
                if (!s_app_started)
                {
                    /* Flash operation initiated during initialization has been completed */
                    app_start();
                    s_app_started = true;
                }
            }
#endif
            break;

        default:
            break;
    }
}

/* Binds the local models correctly with the desired keys */
void app_default_models_bind_setup(void)
{
    health_server_t* p_health_server;
    p_health_server = mesh_stack_health_server_get();

    ERROR_CHECK(dsm_address_subscription_add(HEALTH_GROUP_ADDRESS, &health_subscribe_handle));
    ERROR_CHECK(dsm_address_publish_add(HEALTH_GROUP_ADDRESS, &health_publish_handle));
    ERROR_CHECK(dsm_address_subscription_add(RSSI_UTIL_GROUP_ADDRESS, &rssi_util_subscribe_handle));
    ERROR_CHECK(dsm_address_publish_add(RSSI_UTIL_GROUP_ADDRESS, &rssi_util_publish_handle));
    ERROR_CHECK(dsm_address_publish_add(PROVISIONER_ADDRESS, &rssi_server_publish_handle));

    ERROR_CHECK(access_model_application_bind(p_health_server->model_handle, m_dev_handles.m_appkey_handle));
    ERROR_CHECK(access_model_publish_application_set(p_health_server->model_handle, m_dev_handles.m_appkey_handle));
    ERROR_CHECK(access_model_publish_address_set(p_health_server->model_handle, health_publish_handle));
    ERROR_CHECK(access_model_publish_period_set(p_health_server->model_handle, ACCESS_PUBLISH_RESOLUTION_1S, 1));

    /* Bind health client to App key, and configure publication key */
    ERROR_CHECK(access_model_application_bind(m_dev_handles.m_health_client_instance.model_handle, m_dev_handles.m_appkey_handle));
    ERROR_CHECK(access_model_publish_application_set(m_dev_handles.m_health_client_instance.model_handle, m_dev_handles.m_appkey_handle));
    ERROR_CHECK(access_model_subscription_list_alloc(m_dev_handles.m_health_client_instance.model_handle));
    ERROR_CHECK(access_model_subscription_add(m_dev_handles.m_health_client_instance.model_handle, health_subscribe_handle));

    ERROR_CHECK(access_model_application_bind(m_rssi_util.model_handle, m_dev_handles.m_appkey_handle));
    ERROR_CHECK(access_model_publish_application_set(m_rssi_util.model_handle, m_dev_handles.m_appkey_handle));
    ERROR_CHECK(access_model_subscription_list_alloc(m_rssi_util.model_handle));
    ERROR_CHECK(access_model_subscription_add(m_rssi_util.model_handle, rssi_util_subscribe_handle));
    ERROR_CHECK(access_model_publish_address_set(m_rssi_util.model_handle, rssi_util_publish_handle));

    ERROR_CHECK(access_model_application_bind(m_rssi_server.model_handle, m_dev_handles.m_appkey_handle));
    ERROR_CHECK(access_model_publish_application_set(m_rssi_server.model_handle, m_dev_handles.m_appkey_handle));
    ERROR_CHECK(access_model_publish_address_set(m_rssi_server.model_handle, rssi_server_publish_handle));
    ERROR_CHECK(access_model_publish_period_set(m_rssi_server.model_handle, ACCESS_PUBLISH_RESOLUTION_1S, 10));

    ERROR_CHECK(access_model_application_bind(m_rssi_client.model_handle, m_dev_handles.m_appkey_handle));
    ERROR_CHECK(access_model_publish_application_set(m_rssi_client.model_handle, m_dev_handles.m_appkey_handle));

    /* Bind self-config server to the self device key */
    ERROR_CHECK(config_server_bind(m_dev_handles.m_self_devkey_handle));
}


static bool app_flash_config_load(void)
{
    bool app_load = false;
#if PERSISTENT_STORAGE
    app_flash_manager_add();
    app_load = load_app_data();
#endif
    if (!app_load)
    {
        m_nw_state.provisioned_devices = 0;
        m_nw_state.configured_devices = 0;
        m_nw_state.next_device_address = UNPROV_START_ADDRESS;
        ERROR_CHECK(store_app_data());
    }
    else
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Restored: App data\n");
    }
    return app_load;
}

static void button_event_handler(uint32_t button_number)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Button %u pressed\n", button_number + 1);
    switch (button_number)
    {
        case 0:
        {
            /* Check if all devices have been provisioned or not */
            break;
        }

        /* Initiate node reset */
        case 3:
        {
            /* Clear all the states to reset the node. */
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "----- Node reset -----\n");

            clear_app_data();
            mesh_stack_config_clear();

            hal_led_blink_ms(1 << BSP_LED_1, LED_BLINK_INTERVAL_MS, LED_BLINK_CNT_PROV);
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "----- Press reset button or power cycle the device  -----\n");
            break;
        }

        default:
            break;
    }
}

static void app_rtt_input_handler(int key)
{
    if (key >= '0' && key <= '4')
    {
        uint32_t button_number = key - '0';
        button_event_handler(button_number);
    }
    switch(key)
    {
        case '-':
            config_address_pointer--;

            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "config_address_pointer: %x\n", config_address_pointer);
            break;

        case '+':
            config_address_pointer++;

            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "config_address_pointer: %x\n", config_address_pointer);
            break;

        case 'p':
            start_prov_check(config_address_pointer);
            break;

        case 'c':
            start_config_check(config_address_pointer, m_nw_state.appkey);
            break;

        case 'r':
            current_reset_addr = config_address_pointer;//TODO
            start_reset_check(config_address_pointer);
            break;

        case 'q':
            add_entry(config_address_pointer);
            break;

        case 'w':
            delete_entry(config_address_pointer);
            break;
        default:
            break;
    }
}

void models_init_cb(void)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Initializing and adding models\n");
    m_dev_handles.m_netkey_handle = DSM_HANDLE_INVALID;
    m_dev_handles.m_appkey_handle = DSM_HANDLE_INVALID;
    m_dev_handles.m_self_devkey_handle = DSM_HANDLE_INVALID;

    /* This app requires following models :
     * config client : To be able to configure other devices
     * health client : To be able to interact with other health servers */
    ERROR_CHECK(config_client_init(app_config_client_event_cb));
    ERROR_CHECK(health_client_init(&m_dev_handles.m_health_client_instance, 0, app_health_event_cb));
    ERROR_CHECK(rssi_util_init(&m_rssi_util));
    ERROR_CHECK(rssi_server_init(&m_rssi_server, 0));
    ERROR_CHECK(rssi_client_init(&m_rssi_client, 0, app_health_rssi_cb));
}

static void mesh_init(void)
{
    bool device_provisioned;
    mesh_stack_init_params_t init_params =
    {
        .core.irq_priority       = NRF_MESH_IRQ_PRIORITY_LOWEST,
        .core.lfclksrc           = DEV_BOARD_LF_CLK_CFG,
        .models.models_init_cb   = models_init_cb,
        .models.config_server_cb = app_config_server_event_cb
    };
    ERROR_CHECK(mesh_stack_init(&init_params, &device_provisioned));
    ERROR_CHECK(nrf_mesh_serial_init(NULL));

    nrf_mesh_evt_handler_add(&m_mesh_core_event_handler);

    /* Load application configuration, if available */
    m_dev_handles.flash_load_success = app_flash_config_load();

    /* Initialize the provisioner */
    mesh_provisioner_init_params_t m_prov_helper_init_info =
    {
        .p_dev_data = &m_dev_handles,
        .p_nw_data = &m_nw_state,
        .netkey_idx = NETKEY_INDEX,
        .attention_duration_s = ATTENTION_DURATION_S,
        .p_data_store_cb  = app_data_store_cb,
        .p_prov_success_cb = app_prov_success_cb,
        .p_prov_failed_cb = app_prov_failed_cb
    };
    prov_helper_init(&m_prov_helper_init_info);

    if (!device_provisioned)
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Setup defaults: Adding keys, addresses, and bindings \n");

        prov_helper_provision_self();
        app_default_models_bind_setup();
        access_flash_config_store();
        app_data_store_cb();
    }
    else
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Restored: Handles \n");
        prov_helper_device_handles_load();
    }

    node_setup_cb_set(app_config_successful_cb, app_config_failed_cb);
}

static void initialize(void)
{
    __LOG_INIT(LOG_SRC_APP | LOG_SRC_ACCESS, LOG_LEVEL_INFO, LOG_CALLBACK_DEFAULT);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "----- BLE Mesh Light Switch Provisioner Demo -----\n");

    ERROR_CHECK(app_timer_init());
//    hal_leds_init();
//
//#if BUTTON_BOARD
//    ERROR_CHECK(hal_buttons_init(button_event_handler));
//#endif

    ble_stack_init();

    /* Mesh Init */
    mesh_init();
}

static void app_start(void)
{
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Starting application ...\n");
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Provisoned Nodes: %d, Configured Nodes: %d Next Address: 0x%04x\n",
          m_nw_state.provisioned_devices, m_nw_state.configured_devices, m_nw_state.next_device_address);
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Dev key ", m_nw_state.self_devkey, NRF_MESH_KEY_SIZE);
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Net key ", m_nw_state.netkey, NRF_MESH_KEY_SIZE);
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "App key ", m_nw_state.appkey, NRF_MESH_KEY_SIZE);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Press Button 1 to start provisioning and configuration process. \n");
}

static void start(void)
{
    rtt_input_enable(app_rtt_input_handler, RTT_INPUT_POLL_PERIOD_MS);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "<start> \n");

#if (!PERSISTENT_STORAGE)
    app_start();
#endif

    ERROR_CHECK(nrf_mesh_enable());

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Enabling serial interface...\n");
    ERROR_CHECK(nrf_mesh_serial_enable());

    hal_led_mask_set(LEDS_MASK, LED_MASK_STATE_ON);
    hal_led_blink_ms(LEDS_MASK, LED_BLINK_INTERVAL_MS, LED_BLINK_CNT_START);
}

int main(void)
{
    nrf_gpio_cfg_output(13);
    nrf_gpio_pin_clear(13);
    nrf_gpio_cfg_output(11);
    nrf_gpio_pin_set(11);

    initialize();
    start();

    for (;;)
    {
        (void)sd_app_evt_wait();
    }
}
