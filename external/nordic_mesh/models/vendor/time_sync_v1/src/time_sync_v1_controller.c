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

#ifndef CHANNEL_COMPENSATION_IN_MICRO_SEC
    #define CHANNEL_COMPENSATION_IN_MICRO_SEC 367
#endif

#ifndef LAST_MESH_ADV_CHANNEL
  #define LAST_MESH_ADV_CHANNEL 39
#endif

#ifndef FIRST_MESH_ADV_CHANNEL
  #define FIRST_MESH_ADV_CHANNEL 37
#endif

typedef struct
{
    uint32_t tx_timestamp;
    uint8_t session_tid;
} sync_tx_timestamp_t;

typedef struct
{
    uint8_t tid;
    uint16_t channel_comp; /**< Channel compensation in micro seconds */ 
    uint8_t hop_ctr;
} sync_initial_msg_t;

static time_sync_controller_t* mp_controller;
static nrf_mesh_evt_handler_t m_time_sync_core_evt_handler;


/*For development only*/
static bool m_publish_timer_active;


/* Forward declaration */
static uint32_t send_initial_sync_msg(time_sync_controller_t * p_controller, sync_initial_msg_t init_msg);
static uint32_t send_tx_sender_timestamp(time_sync_controller_t * p_controller, sync_tx_timestamp_t msg);


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
    return (int8_t)(mp_controller->current_session_tid - tid) < 0;
}

/* Calculates the time compensation for which channel the message was recived on */
static uint32_t compensate_for_channel(uint8_t channel, uint16_t compensation_per_chan)
{
    return compensation_per_chan * (channel - FIRST_MESH_ADV_CHANNEL);
}


static void add_timestamp_entry(uint16_t addr, uint32_t timestamp_val, uint8_t session_tid, uint8_t hop_cnt)
{
    /* Checks if a entry already exists */
    for (uint8_t i = 0; i < mp_controller->inital_timestamp_entry_ctr; i++)
    {
        if (mp_controller->inital_timestamp_buffer[i].sender_addr == addr)
        {
            mp_controller->inital_timestamp_buffer[i].timestamp_val = timestamp_val;
            mp_controller->inital_timestamp_buffer[i].session_tid = session_tid;
            return;
        }
    }
    /* Adds a new entry as long as there is room in the buffer */
    if (mp_controller->inital_timestamp_entry_ctr < INITIAL_TIMESTAMP_BUFFER_SIZE)
    {
        mp_controller->inital_timestamp_buffer[mp_controller->inital_timestamp_entry_ctr].sender_addr = addr;
        mp_controller->inital_timestamp_buffer[mp_controller->inital_timestamp_entry_ctr].timestamp_val = timestamp_val;
        mp_controller->inital_timestamp_buffer[mp_controller->inital_timestamp_entry_ctr].session_tid = session_tid;

        mp_controller->inital_timestamp_entry_ctr++;
    }
    else
    {
        /* Reset buffer if buffer gets full */
        mp_controller->inital_timestamp_entry_ctr = 0;
    }
}


/********** Event/timeout handlers **********/

/* Perodic model timeout handler */
static void time_sync_publish_timeout_handler(access_model_handle_t handle, void * p_args) 
{
    if(m_publish_timer_active)
    {
        time_sync_controller_synchronize((time_sync_controller_t *)p_args);
    }
}


/** Event handler callback sends the pending triggered message on the TX complete */
static void time_sync_core_evt_cb(const nrf_mesh_evt_t * p_evt)
{
    switch (p_evt->type)
    {
        case NRF_MESH_EVT_TX_COMPLETE:
            if(mp_controller->current_tx_token == p_evt->params.tx_complete.token)
            {
                sync_tx_timestamp_t msg;
                msg.tx_timestamp = p_evt->params.tx_complete.timestamp - sync_timer_get_offset() - ((LAST_MESH_ADV_CHANNEL - FIRST_MESH_ADV_CHANNEL) * CHANNEL_COMPENSATION_IN_MICRO_SEC);
                msg.session_tid = mp_controller->current_session_tid;
                (void) send_tx_sender_timestamp(mp_controller, msg);
            }
            break;

        default:
            break;
    }
}


/********** Outgoing message functions **********/

static uint32_t send_initial_sync_msg(time_sync_controller_t * p_controller, sync_initial_msg_t init_msg)
{
    mp_controller->current_tx_token = nrf_mesh_unique_token_get();

    access_message_tx_t message;
    message.opcode.opcode = TIME_SYNC_OPCODE_SEND_INIT_SYNC_MSG;
    message.opcode.company_id = ACCESS_COMPANY_ID_NORDIC;
    message.p_buffer = (const uint8_t*) &init_msg;
    message.length = sizeof(init_msg);
    message.force_segmented = false;
    message.transmic_size = NRF_MESH_TRANSMIC_SIZE_DEFAULT;
    message.access_token = mp_controller->current_tx_token;
    
    access_publish_retransmit_t access_publish_retransmit = {NULL};
    (void) access_model_publish_retransmit_set(p_controller->model_handle, access_publish_retransmit);

    (void) access_model_publish_ttl_set(p_controller->model_handle, 0);
    uint32_t error_code = access_model_publish(p_controller->model_handle, &message);

    return error_code;
}


static uint32_t send_tx_sender_timestamp(time_sync_controller_t * p_controller, sync_tx_timestamp_t msg)
{
    access_message_tx_t message;
    message.opcode.opcode = TIME_SYNC_OPCODE_SEND_TX_SENDER_TIMESTAMP;
    message.opcode.company_id = ACCESS_COMPANY_ID_NORDIC;
    message.p_buffer = (const uint8_t*) &msg;
    message.length = sizeof(msg);
    message.force_segmented = false;
    message.transmic_size = NRF_MESH_TRANSMIC_SIZE_DEFAULT;
    message.access_token = nrf_mesh_unique_token_get();

    access_publish_retransmit_t access_publish_retransmit = {NULL};
    (void) access_model_publish_retransmit_set(p_controller->model_handle, access_publish_retransmit);

    (void) access_model_publish_ttl_set(p_controller->model_handle, 0);
    uint32_t error_code = access_model_publish(p_controller->model_handle, &message);

    return error_code;
}


/********** Incoming message handlers **********/

static void handle_msg_initial_sync(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    if((p_message->meta_data.p_core_metadata->source == NRF_MESH_RX_SOURCE_SCANNER) && (p_message->length == sizeof(sync_initial_msg_t)))
    {
        sync_initial_msg_t incoming_msg;
        memcpy(&incoming_msg, p_message->p_data, p_message->length);

        if(is_higher_tid(incoming_msg.tid) && (!mp_controller->is_master))
        {
            uint32_t reciver_timestamp = p_message->meta_data.p_core_metadata->params.scanner.timestamp;
            uint32_t channel_compensation = compensate_for_channel(p_message->meta_data.p_core_metadata->params.scanner.channel, incoming_msg.channel_comp);
            add_timestamp_entry(p_message->meta_data.src.value, reciver_timestamp - channel_compensation, incoming_msg.tid, incoming_msg.hop_ctr);
        }
    }
}


static void handle_msg_tx_sender_timestamp(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    if((p_message->meta_data.p_core_metadata->source == NRF_MESH_RX_SOURCE_SCANNER) && (p_message->length == sizeof(sync_tx_timestamp_t)))
    {
        sync_tx_timestamp_t sender_timestamp;
        memcpy(&sender_timestamp, p_message->p_data, p_message->length);

        /* Searches through the buffer for a matching entry */
        for (uint8_t i = 0; i < mp_controller->inital_timestamp_entry_ctr; i++)
        {
            if ((mp_controller->inital_timestamp_buffer[i].sender_addr == p_message->meta_data.src.value) && (mp_controller->inital_timestamp_buffer[i].session_tid == sender_timestamp.session_tid) && (!mp_controller->is_master))
            {
                sync_timer_set_timer_offset(mp_controller->inital_timestamp_buffer[i].timestamp_val - sender_timestamp.tx_timestamp);

                //TODO: Remove log when ready
                __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Recieved a sync update. Offset set to %d\n", mp_controller->inital_timestamp_buffer[i].timestamp_val - sender_timestamp.tx_timestamp);

                /*Reset the buffer*/
                mp_controller->inital_timestamp_entry_ctr = 0;
                
                mp_controller->current_session_tid = sender_timestamp.session_tid;

                /*Increment the hop count for the next iteration in the time sync tree*/
                uint8_t new_hop_count_val = mp_controller->inital_timestamp_buffer[i].hop_ctr + 1;

                sync_initial_msg_t init_msg;
                init_msg.tid = mp_controller->current_session_tid;
                init_msg.channel_comp = CHANNEL_COMPENSATION_IN_MICRO_SEC;
                init_msg.hop_ctr = new_hop_count_val;
                (void) send_initial_sync_msg(mp_controller, init_msg);
            }
        }
    }
}


static void handle_msg_reset(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    if(p_message->meta_data.p_core_metadata->source == NRF_MESH_RX_SOURCE_SCANNER)
    {
        /* Resets the device's session tid*/
        mp_controller->current_session_tid = 0;
        mp_controller->is_master = false;

        //TODO: Remove log when ready
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "handle_msg_reset\n");
    }
}

/********** Opcode handler list *********/

static const access_opcode_handler_t m_opcode_handlers[] =
{
    { ACCESS_OPCODE_VENDOR(TIME_SYNC_OPCODE_SEND_TX_SENDER_TIMESTAMP, ACCESS_COMPANY_ID_NORDIC),   handle_msg_tx_sender_timestamp },
    { ACCESS_OPCODE_VENDOR(TIME_SYNC_OPCODE_SEND_INIT_SYNC_MSG, ACCESS_COMPANY_ID_NORDIC),   handle_msg_initial_sync },
    { ACCESS_OPCODE_VENDOR(TIME_SYNC_OPCODE_RESET, ACCESS_COMPANY_ID_NORDIC),   handle_msg_reset },
};


/********** Interface functions **********/


uint32_t time_sync_controller_init(time_sync_controller_t * p_controller, uint16_t element_index)
{
    if (p_controller == NULL)
    {
        return NRF_ERROR_NULL;
    }
    else if(mp_controller != NULL)
    {
        return NRF_ERROR_FORBIDDEN;
    }
    mp_controller = p_controller;
    access_model_add_params_t add_params =
    {
        .element_index = element_index,
        .model_id.model_id = TIME_SYNC_CONTROLLER_MODEL_ID, /*lint !e64 Type Mismatch */
        .model_id.company_id = ACCESS_COMPANY_ID_NORDIC,
        .p_opcode_handlers = m_opcode_handlers,
        .opcode_count = ARRAY_SIZE(m_opcode_handlers),
        .publish_timeout_cb = time_sync_publish_timeout_handler,
        .p_args = p_controller
    };

    m_time_sync_core_evt_handler.evt_cb = time_sync_core_evt_cb;
    event_handler_add(&m_time_sync_core_evt_handler);
    uint32_t error_code = access_model_add(&add_params, &p_controller->model_handle);
    return error_code;
}


void time_sync_controller_reset(time_sync_controller_t * p_controller)
{
    access_message_tx_t message;
    message.opcode.opcode = TIME_SYNC_OPCODE_RESET;
    message.opcode.company_id = ACCESS_COMPANY_ID_NORDIC;
    message.p_buffer = NULL;
    message.length = 0;
    message.force_segmented = false;
    message.transmic_size = NRF_MESH_TRANSMIC_SIZE_DEFAULT;
    message.access_token = nrf_mesh_unique_token_get();
    
    (void) access_model_publish_ttl_set(p_controller->model_handle, NRF_MESH_TTL_MAX);

    (void) access_model_publish(p_controller->model_handle, &message);
}


void time_sync_controller_synchronize(time_sync_controller_t * p_controller)
{
    /*If the sending device is not master from before we need to make sure that the system is fully reset before we can start syncing*/
    if(!mp_controller->is_master)
    {
        mp_controller->is_master = true;
        mp_controller->current_session_tid = 0;
        time_sync_controller_reset(p_controller);
    }

    mp_controller->current_session_tid++;

    sync_initial_msg_t init_msg;
    init_msg.tid = mp_controller->current_session_tid;
    init_msg.channel_comp = CHANNEL_COMPENSATION_IN_MICRO_SEC;
    init_msg.hop_ctr = 0;
    (void) send_initial_sync_msg(p_controller, init_msg);

}


