#include <string.h>
#include "rssi_client.h" 
#include "rssi_common.h"
#include "mesh_app_utils.h"

/********** Incoming message handlers **********/

/* Function used to send a acknowledgement messages to confirm that the rssi data has arrived */
static uint32_t reply_status(const rssi_client_t * p_client, const access_message_rx_t * p_message)
{
    access_message_tx_t reply;
    reply.opcode.opcode = RSSI_OPCODE_RSSI_ACK;
    reply.opcode.company_id = ACCESS_COMPANY_ID_NORDIC;
    reply.p_buffer = NULL;
    reply.length = 0;
    reply.force_segmented = false;
    reply.transmic_size = NRF_MESH_TRANSMIC_SIZE_DEFAULT;

    return access_model_reply(p_client->model_handle, p_message, &reply);
}

/* Handles incoming rssi data from the servers */
static void handle_rssi_data(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    const rssi_client_t * p_client = p_args;

    p_client->rssi_handler(p_message);

    (void)reply_status(p_client, p_message);
} 

static const access_opcode_handler_t m_opcode_handlers[] =
{
    { ACCESS_OPCODE_VENDOR(RSSI_OPCODE_SEND_RSSI_DATA, ACCESS_COMPANY_ID_NORDIC), handle_rssi_data },
};


/********** Interface functions **********/

uint32_t rssi_client_init(rssi_client_t * p_client, uint16_t element_index, rssi_evt_cb_t rssi_handler)
{
    if ((rssi_handler == NULL) || (p_client == NULL))
    {
        return NRF_ERROR_NULL;
    }

    p_client->rssi_handler = rssi_handler;
    access_model_add_params_t add_params =
    {
        .element_index = element_index,
        .model_id.model_id = RSSI_CLIENT_MODEL_ID, /*lint !e64 Type Mismatch */
        .model_id.company_id = ACCESS_COMPANY_ID_NORDIC,
        .p_opcode_handlers = m_opcode_handlers,
        .opcode_count = sizeof(m_opcode_handlers) / sizeof(m_opcode_handlers[0]),
        .p_args = p_client
    };
    return access_model_add(&add_params, &p_client->model_handle);
}
