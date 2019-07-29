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
#include "access_config.h"



typedef struct
{
    uint32_t tx_timestamp;
    uint32_t token;
} sync_tx_timestamp_t;

typedef struct
{
    uint16_t sender_addr;
    uint32_t timestamp_val;
} timestamp_buffer_entry_t;

static const uint32_t CHANNEL_COMPENSATION = 367;
static const uint8_t LAST_ADV_CHANNEL = 39;

static nrf_mesh_evt_handler_t m_time_sync_core_evt_handler;
static nrf_mesh_tx_token_t m_own_token;
//static uint32_t m_reciver_timestamp;
//static uint32_t m_incoming_tid;
static time_sync_controller_t* mp_controller;
static bool m_initial_timestamp_sent;
static bool m_publish_timer_active;
//static uint32_t m_channel_compensation;
static uint8_t m_outgoing_tid;

//static uint8_t m_own_session_tid;

static uint8_t m_current_received_session_tid;
static uint8_t m_prev_received_session_tid;
//static uint16_t m_senders_addr; 
static bool m_is_master;

static timestamp_buffer_entry_t inital_timestamp_buffer[10];
static uint8_t inital_timestamp_entry_ctr;


/* Forward declaration */
static uint32_t send_initial_sync_msg(time_sync_controller_t * p_server, uint8_t session_tid);
static uint32_t send_tx_sender_timestamp(time_sync_controller_t * p_server, uint32_t tx_timestamp, uint8_t token);
static void set_as_master(void);


//TODO Remove or make better
void send_timestamp(void)
{
    m_outgoing_tid++;
    send_initial_sync_msg(mp_controller, m_outgoing_tid);
}


//TODO Remove or make better
void sync_set_pub_timer(bool on_off)
{
    m_publish_timer_active = on_off;
    set_as_master();
    ERROR_CHECK(access_model_publish_period_set(mp_controller->model_handle, ACCESS_PUBLISH_RESOLUTION_100MS, 2));
}


static bool is_higher_tid(uint8_t tid)
{
    if(abs(tid - m_current_received_session_tid) > UINT8_MAX / 2)
    {
        return true;
    }
    else if(m_current_received_session_tid < tid)
    {
        return true;
    }
    else
    {
        return false;
    }
}


static void add_timestamp_entry(uint16_t addr, uint32_t timestamp_val)
{
        /* Checks if a entry already exists */
    for (uint8_t i = 0; i < inital_timestamp_entry_ctr; i++)
    {
        if (inital_timestamp_buffer[i].sender_addr == addr)
        {
            inital_timestamp_buffer[i].timestamp_val = timestamp_val;
            return;
        }
    }
    /* Adds a new entry as long as there is room in the buffer */
    if (inital_timestamp_entry_ctr < 10)
    {
        inital_timestamp_buffer[inital_timestamp_entry_ctr].sender_addr = addr;
        inital_timestamp_buffer[inital_timestamp_entry_ctr].timestamp_val = timestamp_val;

        inital_timestamp_entry_ctr++;
    }
    else
    {
        /* Reset buffer if buffer gets full */
        inital_timestamp_entry_ctr = 0;
    }
}


static void set_as_master(void)
{
    m_is_master = true;
}

/*Calculates the time compensation for which channel the packet was recived on*/
static uint32_t compensate_for_channel(uint8_t channel)
{
    return CHANNEL_COMPENSATION * (LAST_ADV_CHANNEL - channel);
}



/* Perodic model timeout handle */
static void time_sync_publish_timeout_handler(access_model_handle_t handle, void * p_args) 
{
    if(m_publish_timer_active)
    {
        m_outgoing_tid++;
        send_initial_sync_msg((time_sync_controller_t *)p_args, m_outgoing_tid);
    }
}


/** Event handler callback sends the pending triggered message on the TX complete */
static void time_sync_core_evt_cb(const nrf_mesh_evt_t * p_evt)
{
    switch (p_evt->type)
    {
        case NRF_MESH_EVT_TX_COMPLETE:
            if(m_own_token == p_evt->params.tx_complete.token && m_initial_timestamp_sent)
            {
                static uint32_t last_found_token = 0xFFFFFFFF;
                if (last_found_token == m_own_token) 
                {
                  __LOG(LOG_SRC_APP, LOG_LEVEL_ERROR, "################# GOT SAME TOKEN TWICE: %u\n", m_own_token);
                }
                last_found_token = m_own_token;
                send_tx_sender_timestamp(mp_controller, p_evt->params.tx_complete.timestamp - sync_timer_get_current_offset(), m_outgoing_tid);
            }
            break;

        default:
            break;
    }
}
/*---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

static uint32_t send_initial_sync_msg(time_sync_controller_t * p_server, uint8_t session_tid)
{
    m_own_token = nrf_mesh_unique_token_get();

    access_message_tx_t message;
    message.opcode.opcode = TIME_SYNC_OPCODE_SEND_INIT_SYNC_MSG;
    message.opcode.company_id = ACCESS_COMPANY_ID_NORDIC;
    message.p_buffer = (const uint8_t*) &session_tid;
    message.length = sizeof(session_tid);
    message.force_segmented = false;
    message.transmic_size = NRF_MESH_TRANSMIC_SIZE_DEFAULT;
    message.access_token = m_own_token;
    
    access_model_publish_ttl_set(p_server->model_handle, 0);
    uint32_t error_code = access_model_publish(p_server->model_handle, &message);
    m_initial_timestamp_sent = true;
    return error_code;
}


static void handle_msg_initial_sync(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{

    if(p_message->meta_data.p_core_metadata->source == NRF_MESH_RX_SOURCE_SCANNER && !m_is_master)
    {
        uint8_t incoming_tid;
        memcpy(&incoming_tid, p_message->p_data, p_message->length);

        if(is_higher_tid(incoming_tid))
        {
            __LOG(LOG_SRC_APP, LOG_LEVEL_ERROR, "incoming tid: %d, prev tid: %d\n", incoming_tid, m_prev_received_session_tid);

            m_current_received_session_tid = incoming_tid;

            uint32_t reciver_timestamp = p_message->meta_data.p_core_metadata->params.scanner.timestamp;
            uint32_t channel_compensation = compensate_for_channel(p_message->meta_data.p_core_metadata->params.scanner.channel);
            add_timestamp_entry(p_message->meta_data.src.value, reciver_timestamp + channel_compensation);
        }

    }
}


static uint32_t send_tx_sender_timestamp(time_sync_controller_t * p_server, uint32_t tx_timestamp, uint8_t session_tid)
{
    sync_tx_timestamp_t msg;
    msg.tx_timestamp = tx_timestamp;
    msg.token = session_tid;
    __LOG(LOG_SRC_APP, LOG_LEVEL_ERROR, "send_tx_sender_timestamp\n");

    access_message_tx_t message;
    message.opcode.opcode = TIME_SYNC_OPCODE_SEND_TX_SENDER_TIMESTAMP;
    message.opcode.company_id = ACCESS_COMPANY_ID_NORDIC;
    message.p_buffer = (const uint8_t*) &msg;
    message.length = sizeof(msg);
    message.force_segmented = false;
    message.transmic_size = NRF_MESH_TRANSMIC_SIZE_DEFAULT;
    message.access_token = nrf_mesh_unique_token_get();

    access_model_publish_ttl_set(p_server->model_handle, 0);
    uint32_t error_code = access_model_publish(p_server->model_handle, &message);
    return error_code;
}


static void handle_msg_tx_sender_timestamp(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    if(p_message->meta_data.p_core_metadata->source == NRF_MESH_RX_SOURCE_SCANNER && !m_is_master)
    {
        sync_tx_timestamp_t sender_timestamp;
        memcpy(&sender_timestamp, p_message->p_data, p_message->length);

        if(m_current_received_session_tid == sender_timestamp.token)
        {
            /* Checks if a entry already exists */
            for (uint8_t i = 0; i < inital_timestamp_entry_ctr; i++)
            {
                if (inital_timestamp_buffer[i].sender_addr == p_message->meta_data.src.value)
                {
                    sync_timer_set_timer_offset(inital_timestamp_buffer[i].timestamp_val - sender_timestamp.tx_timestamp);
                    m_prev_received_session_tid = m_current_received_session_tid;

                    /*Reset the buffer*/
                    inital_timestamp_entry_ctr = 0;

                    send_initial_sync_msg(mp_controller, m_prev_received_session_tid);

                    sync_event_t sync_event;
                    mp_controller->time_sync_controller_handler(sync_event);
                }
            }
        } else {
            m_current_received_session_tid = m_prev_received_session_tid;
        }
    }
}


static const access_opcode_handler_t m_opcode_handlers[] =
{
    { ACCESS_OPCODE_VENDOR(TIME_SYNC_OPCODE_SEND_TX_SENDER_TIMESTAMP, ACCESS_COMPANY_ID_NORDIC),   handle_msg_tx_sender_timestamp },
    { ACCESS_OPCODE_VENDOR(TIME_SYNC_OPCODE_SEND_INIT_SYNC_MSG, ACCESS_COMPANY_ID_NORDIC),   handle_msg_initial_sync },
};


/********** Interface functions **********/


uint32_t time_sync_controller_init(time_sync_controller_t * p_server, uint16_t element_index, time_sync_controller_evt_cb_t time_sync_controller_handler)
{
    if (p_server == NULL)
    {
        return NRF_ERROR_NULL;
    }   
    mp_controller = p_server;
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
    uint32_t error_code = access_model_add(&add_params, &p_server->model_handle);
    return error_code;
}


