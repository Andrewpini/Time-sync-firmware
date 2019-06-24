#include "time_sync_v1_controller.h"
#include "nrf_mesh_assert.h"
#include "access_reliable.h"
#include "time_sync_v1_common.h" 
#include "log.h"
#include "mesh_app_utils.h"
#include "nordic_common.h"
#include "access_utils.h"

static bool m_active_transfer; /**< Indicates if there is an active transfer going on between the controllers */

static void time_sync_publish_timeout_handler(access_model_handle_t handle, void * p_args) 
{

}
 
static void reliable_status_cb(access_model_handle_t model_handle, void * p_args, access_reliable_status_t status)
{
    switch (status)
    {
        case ACCESS_RELIABLE_TRANSFER_SUCCESS:
            break;

        case ACCESS_RELIABLE_TRANSFER_TIMEOUT:
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Reliable transfer of RSSI data has timed out\n");
            break;

        case ACCESS_RELIABLE_TRANSFER_CANCELLED:
            break;

        default:
            /* Should not be possible. */
            NRF_MESH_ASSERT(false);
            break;
    }

    /* Allow new data to be sent */
    m_active_transfer = false;
}

static uint32_t send_reliable_message(const time_sync_controller_t * p_server, uint16_t opcode, const uint8_t * p_data, uint16_t length)
{
    const access_reliable_t reliable = {
        .model_handle = p_server->model_handle,
        .message = {
            .p_buffer = p_data,
            .length = length,
            .opcode = ACCESS_OPCODE_VENDOR(opcode, ACCESS_COMPANY_ID_NORDIC),
            .force_segmented = false,
            .transmic_size = NRF_MESH_TRANSMIC_SIZE_DEFAULT,
        },
        .reply_opcode = {
            .opcode = TIME_SYNC_OPCODE_ACK,
            .company_id = ACCESS_COMPANY_ID_NORDIC,
        },
        .timeout = SEC_TO_US(30),
        .status_cb = reliable_status_cb,
    };

    return access_model_reliable_publish(&reliable);
}



/* Handler needed for the acknowledgement messages */
static void acknowledgement_handler(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    
} 

static const access_opcode_handler_t m_opcode_handlers[] =
{
    { ACCESS_OPCODE_VENDOR(TIME_SYNC_OPCODE_ACK, ACCESS_COMPANY_ID_NORDIC),   acknowledgement_handler },
};

/********** Interface functions **********/

uint32_t time_sync_controller_init(time_sync_controller_t * p_server, uint16_t element_index, time_sync_controller_evt_cb_t time_sync_controller_handler)
{
    if (p_server == NULL)
    {
        return NRF_ERROR_NULL;
    }   

    p_server->time_sync_controller_handler = time_sync_controller_handler;
    access_model_add_params_t add_params =
    {
        .element_index = element_index,
        .model_id.model_id = TIME_SYNC_CONTROLLER_MODEL_ID, /*lint !e64 Type Mismatch */
        .model_id.company_id = ACCESS_COMPANY_ID_NORDIC,
        .p_opcode_handlers = m_opcode_handlers,
        .opcode_count = ARRAY_SIZE(m_opcode_handlers),
        .publish_timeout_cb = time_sync_publish_timeout_handler,
        .p_args = p_server
    };
    return access_model_add(&add_params, &p_server->model_handle);
}

