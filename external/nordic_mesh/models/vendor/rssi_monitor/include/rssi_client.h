#ifndef RSSI_CLIENT_H__
#define RSSI_CLIENT_H__

#include <stdbool.h>
#include <stdint.h>
 
#include "access.h"
#include "rssi_common.h"

/** Object type for rssi client instances */
typedef struct rssi_client_t rssi_client_t;

/** Event callback function type */ 
typedef void (*rssi_evt_cb_t)(const access_message_rx_t* p_message);

/** Rssi client instance structure */
struct rssi_client_t
{
    access_model_handle_t  model_handle;
    rssi_evt_cb_t rssi_handler;
};

/** Initializes a rssi client instance.
 *
 * @param[in,out] p_client      Pointer to the client instance structure.
 * @param[in]     element_index Index of the element to register the model with.
 * @param[in]     rssi_handler  Handler used to process incoming messages.
 *
 * @retval NRF_SUCCESS      The rssi client was successfully initialized.
 * @retval NRF_ERROR_NULL   Passed rssi_handler not valid
 * 
 * @see access_model_add() for other return values
 */
uint32_t rssi_client_init(rssi_client_t * p_client, uint16_t element_index, rssi_evt_cb_t rssi_handler);

#endif

