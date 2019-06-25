#include "time_sync_v1_controller.h"
#include "nrf_mesh_assert.h"
#include "access_reliable.h"
#include "time_sync_v1_common.h" 
#include "log.h"
#include "mesh_app_utils.h"
#include "nordic_common.h"
#include "access_utils.h"
#include "event.h"
#include "nrf_mesh.h"
#include "timeslot_timer.h"
#include "sync_timer_handler.h"

static bool m_active_transfer; /**< Indicates if there is an active transfer going on between the controllers */

/* Forward declaration */
static uint32_t send_reliable_message(const time_sync_controller_t * p_server, uint16_t opcode, const uint8_t * p_data, uint16_t length);
static uint32_t send_own_timestamp(time_sync_controller_t * p_server);

typedef struct
{
    uint32_t initial_timestamp;
    uint32_t end_timestamp;
    uint32_t diff;
} time_snapshot_t;

static nrf_mesh_evt_handler_t m_time_sync_core_evt_handler;
static nrf_mesh_tx_token_t m_current_token;
static time_snapshot_t m_time_outgoing_snapshot;
static time_snapshot_t m_time_incoming_snapshot;


/** Event handler callback sends the pending triggered message on the TX complete */
static void time_sync_core_evt_cb(const nrf_mesh_evt_t * p_evt)
{
    switch (p_evt->type)
    {
        case NRF_MESH_EVT_TX_COMPLETE:
            if(m_current_token == p_evt->params.tx_complete.token)
            {
                m_time_outgoing_snapshot.end_timestamp = sync_timer_get_adjusted_timestamp();
                m_time_outgoing_snapshot.diff = m_time_outgoing_snapshot.end_timestamp - m_time_outgoing_snapshot.initial_timestamp;
                m_current_token = NULL;

//                __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "OUTGOING: (Initial timestamp: %d, end_timestamp: %d, Diff: %d)\n", m_time_outgoing_snapshot.initial_timestamp, m_time_outgoing_snapshot.end_timestamp,m_time_outgoing_snapshot.diff);
            }
            break;

        default:
            break;
    }
}

static void time_sync_publish_timeout_handler(access_model_handle_t handle, void * p_args) 
{
    uint32_t error_code = send_own_timestamp((time_sync_controller_t *)p_args);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "timestamp: %d)\n",sync_timer_get_adjusted_timestamp());

    if (error_code == NRF_SUCCESS)
    {
        m_active_transfer = true;
    }
}
 
//static void reliable_status_cb(access_model_handle_t model_handle, void * p_args, access_reliable_status_t status)
//{
//    switch (status)
//    {
//        case ACCESS_RELIABLE_TRANSFER_SUCCESS:
//            break;
//
//        case ACCESS_RELIABLE_TRANSFER_TIMEOUT:
//            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Reliable transfer of RSSI data has timed out\n");
//            break;
//
//        case ACCESS_RELIABLE_TRANSFER_CANCELLED:
//            break;
//
//        default:
//            /* Should not be possible. */
//            NRF_MESH_ASSERT(false);
//            break;
//    }
// 
//    /* Allow new data to be sent */
//    m_active_transfer = false;
//}

/* Sends a database beacon to nearby nodes */
static uint32_t send_own_timestamp(time_sync_controller_t * p_server)
{
    uint32_t own_timestamp = sync_timer_get_adjusted_timestamp();

    access_message_tx_t message;
    message.opcode.opcode = TIME_SYNC_OPCODE_SEND_OWN_TIMESTAMP;
    message.opcode.company_id = ACCESS_COMPANY_ID_NORDIC;
    message.p_buffer = (const uint8_t*) &own_timestamp;
    message.length = sizeof(own_timestamp);
    message.force_segmented = false;
    message.transmic_size = NRF_MESH_TRANSMIC_SIZE_DEFAULT;

    m_current_token = nrf_mesh_unique_token_get();
    message.access_token = m_current_token;

    m_time_outgoing_snapshot.initial_timestamp = own_timestamp;
    uint32_t error_code = access_model_publish(p_server->model_handle, &message);
   
    return error_code;
}

//static uint32_t send_reliable_message(const time_sync_controller_t * p_server, uint16_t opcode, const uint8_t * p_data, uint16_t length)
//{
//    const access_reliable_t reliable = {
//        .model_handle = p_server->model_handle,
//        .message = {
//            .p_buffer = p_data,
//            .length = length,
//            .opcode = ACCESS_OPCODE_VENDOR(opcode, ACCESS_COMPANY_ID_NORDIC),
//            .force_segmented = false,
//            .transmic_size = NRF_MESH_TRANSMIC_SIZE_DEFAULT,
//            .access_token = nrf_mesh_unique_token_get(),
//        },
//        .reply_opcode = {
//            .opcode = TIME_SYNC_OPCODE_ACK,
//            .company_id = ACCESS_COMPANY_ID_NORDIC,
//        },
//        .timeout = SEC_TO_US(30),
//        .status_cb = reliable_status_cb,
//    };
//
//    NRF_TIMER3->TASKS_CAPTURE[1] = 1;
//    m_time_outgoing_snapshot.initial_timestamp = NRF_TIMER3->CC[1];
//    m_current_token = reliable.message.access_token;
//    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "TOKEN: %d\n", reliable.message.access_token);
//    return access_model_reliable_publish(&reliable);
//}



///* Handler needed for the acknowledgement messages */
//static void acknowledgement_handler(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
//{
//    
//} 

static void handle_incoming_timestamp(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    if(p_message->meta_data.p_core_metadata->source == NRF_MESH_RX_SOURCE_SCANNER)
    {
        uint32_t incoming_timestamp;
        memcpy(&incoming_timestamp, p_message->p_data, p_message->length);

        m_time_incoming_snapshot.initial_timestamp = p_message->meta_data.p_core_metadata->params.scanner.timestamp;
        m_time_incoming_snapshot.end_timestamp = sync_timer_get_adjusted_timestamp();
        m_time_incoming_snapshot.diff = m_time_incoming_snapshot.end_timestamp - m_time_incoming_snapshot.initial_timestamp;

//        sync_timer_set_timer_offset(incoming_timestamp + m_time_incoming_snapshot.diff);
//        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "INCOMING: (Initial timestamp: %d, end_timestamp: %d, Diff: %d)\n", m_time_incoming_snapshot.initial_timestamp, m_time_incoming_snapshot.end_timestamp, m_time_incoming_snapshot.diff);
    }
}

static const access_opcode_handler_t m_opcode_handlers[] =
{
//    { ACCESS_OPCODE_VENDOR(TIME_SYNC_OPCODE_ACK, ACCESS_COMPANY_ID_NORDIC),   acknowledgement_handler },
    { ACCESS_OPCODE_VENDOR(TIME_SYNC_OPCODE_SEND_OWN_TIMESTAMP, ACCESS_COMPANY_ID_NORDIC),   handle_incoming_timestamp },
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

    m_time_sync_core_evt_handler.evt_cb = time_sync_core_evt_cb;
    event_handler_add(&m_time_sync_core_evt_handler);
    return access_model_add(&add_params, &p_server->model_handle);
}

