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

#define INITIAL_TIMESTAMP_BUFFER_SIZE 10
#define CHANNEL_COMPENSATION_IN_MICRO_SEC 367
#define LAST_MESH_ADV_CHANNEL 39

typedef struct
{
    uint32_t tx_timestamp;
    uint32_t session_tid;
} sync_tx_timestamp_t;

typedef struct
{
    uint16_t sender_addr;
    uint32_t timestamp_val;
    uint8_t session_tid;
} timestamp_buffer_entry_t;


static time_sync_controller_t* mp_controller;
static nrf_mesh_evt_handler_t m_time_sync_core_evt_handler;
static timestamp_buffer_entry_t m_inital_timestamp_buffer[INITIAL_TIMESTAMP_BUFFER_SIZE];


static nrf_mesh_tx_token_t m_current_tx_token;
static uint8_t m_current_session_tid;
static uint8_t m_inital_timestamp_entry_ctr;

/*For development only*/
static bool m_publish_timer_active;


/* Forward declaration */
static uint32_t send_initial_sync_msg(time_sync_controller_t * p_server, uint8_t session_tid);
static uint32_t send_tx_sender_timestamp(time_sync_controller_t * p_server, uint32_t tx_timestamp, uint8_t token);


/********** Development functions **********/

//TODO Remove or make better
void sync_set_pub_timer(bool on_off)
{
    m_publish_timer_active = on_off;
}

/********** Utility functions **********/

/* Used to determine if a tid is greater than the current one */
static bool is_higher_tid(uint8_t tid)
{
    if(abs(tid - m_current_session_tid) > (UINT8_MAX / 2))
    {
        return true;
    }
    else if(m_current_session_tid < tid)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/* Calculates the time compensation for which channel the packet was recived on */
static uint32_t compensate_for_channel(uint8_t channel)
{
    return CHANNEL_COMPENSATION_IN_MICRO_SEC * (LAST_MESH_ADV_CHANNEL - channel);
}


static void add_timestamp_entry(uint16_t addr, uint32_t timestamp_val, uint8_t session_tid)
{
    /* Checks if a entry already exists */
    for (uint8_t i = 0; i < m_inital_timestamp_entry_ctr; i++)
    {
        if (m_inital_timestamp_buffer[i].sender_addr == addr)
        {
            m_inital_timestamp_buffer[i].timestamp_val = timestamp_val;
            m_inital_timestamp_buffer[i].session_tid = session_tid;
            return;
        }
    }
    /* Adds a new entry as long as there is room in the buffer */
    if (m_inital_timestamp_entry_ctr < INITIAL_TIMESTAMP_BUFFER_SIZE)
    {
        m_inital_timestamp_buffer[m_inital_timestamp_entry_ctr].sender_addr = addr;
        m_inital_timestamp_buffer[m_inital_timestamp_entry_ctr].timestamp_val = timestamp_val;
        m_inital_timestamp_buffer[m_inital_timestamp_entry_ctr].session_tid = session_tid;

        m_inital_timestamp_entry_ctr++;
    }
    else
    {
        /* Reset buffer if buffer gets full */
        m_inital_timestamp_entry_ctr = 0;
    }
}


/********** Event/timeout handlers **********/

/* Perodic model timeout handler */
static void time_sync_publish_timeout_handler(access_model_handle_t handle, void * p_args) 
{
    if(m_publish_timer_active)
    {
        m_current_session_tid++;
        send_initial_sync_msg((time_sync_controller_t *)p_args, m_current_session_tid);
    }
}


/** Event handler callback sends the pending triggered message on the TX complete */
static void time_sync_core_evt_cb(const nrf_mesh_evt_t * p_evt)
{
    switch (p_evt->type)
    {
        case NRF_MESH_EVT_TX_COMPLETE:
            if(m_current_tx_token == p_evt->params.tx_complete.token)
            {
                send_tx_sender_timestamp(mp_controller, p_evt->params.tx_complete.timestamp - sync_timer_get_current_offset(), m_current_session_tid);
            }
            break;

        default:
            break;
    }
}


/********** Outgoing message functions **********/

static uint32_t send_initial_sync_msg(time_sync_controller_t * p_server, uint8_t session_tid)
{
    m_current_tx_token = nrf_mesh_unique_token_get();

    access_message_tx_t message;
    message.opcode.opcode = TIME_SYNC_OPCODE_SEND_INIT_SYNC_MSG;
    message.opcode.company_id = ACCESS_COMPANY_ID_NORDIC;
    message.p_buffer = (const uint8_t*) &session_tid;
    message.length = sizeof(session_tid);
    message.force_segmented = false;
    message.transmic_size = NRF_MESH_TRANSMIC_SIZE_DEFAULT;
    message.access_token = m_current_tx_token;
    
    access_model_publish_ttl_set(p_server->model_handle, 0);
    uint32_t error_code = access_model_publish(p_server->model_handle, &message);

    return error_code;
}


static uint32_t send_tx_sender_timestamp(time_sync_controller_t * p_server, uint32_t tx_timestamp, uint8_t session_tid)
{
    sync_tx_timestamp_t msg;
    msg.tx_timestamp = tx_timestamp;
    msg.session_tid = session_tid;

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


/********** Incoming message handlers **********/

static void handle_msg_initial_sync(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    if(p_message->meta_data.p_core_metadata->source == NRF_MESH_RX_SOURCE_SCANNER)
    {
        uint8_t incoming_tid;
        memcpy(&incoming_tid, p_message->p_data, p_message->length);

        if(is_higher_tid(incoming_tid))
        {
            uint32_t reciver_timestamp = p_message->meta_data.p_core_metadata->params.scanner.timestamp;
            uint32_t channel_compensation = compensate_for_channel(p_message->meta_data.p_core_metadata->params.scanner.channel);
            add_timestamp_entry(p_message->meta_data.src.value, reciver_timestamp + channel_compensation, incoming_tid);
        }
    }
}


static void handle_msg_tx_sender_timestamp(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    if(p_message->meta_data.p_core_metadata->source == NRF_MESH_RX_SOURCE_SCANNER)
    {
        sync_tx_timestamp_t sender_timestamp;
        memcpy(&sender_timestamp, p_message->p_data, p_message->length);

        /* Searches through the buffer for a matching entry */
        for (uint8_t i = 0; i < m_inital_timestamp_entry_ctr; i++)
        {
            if ((m_inital_timestamp_buffer[i].sender_addr == p_message->meta_data.src.value) && (m_inital_timestamp_buffer[i].session_tid == sender_timestamp.session_tid))
            {
                sync_timer_set_timer_offset(m_inital_timestamp_buffer[i].timestamp_val - sender_timestamp.tx_timestamp);

                /*Reset the buffer*/
                m_inital_timestamp_entry_ctr = 0;
                
                m_current_session_tid = sender_timestamp.session_tid;
                send_initial_sync_msg(mp_controller, m_current_session_tid);
            }
        }
    }
}

/********** Opcode handler list *********/

static const access_opcode_handler_t m_opcode_handlers[] =
{
    { ACCESS_OPCODE_VENDOR(TIME_SYNC_OPCODE_SEND_TX_SENDER_TIMESTAMP, ACCESS_COMPANY_ID_NORDIC),   handle_msg_tx_sender_timestamp },
    { ACCESS_OPCODE_VENDOR(TIME_SYNC_OPCODE_SEND_INIT_SYNC_MSG, ACCESS_COMPANY_ID_NORDIC),   handle_msg_initial_sync },
};


/********** Interface functions **********/


uint32_t time_sync_controller_init(time_sync_controller_t * p_server, uint16_t element_index)
{
    if (p_server == NULL)
    {
        return NRF_ERROR_NULL;
    }   
    mp_controller = p_server;
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

void time_sync_controller_synchronize(void)
{
    m_current_session_tid++;
    send_initial_sync_msg(mp_controller, m_current_session_tid);
}


