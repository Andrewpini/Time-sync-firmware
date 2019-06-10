#ifndef RSSI_SERVER_H__
#define RSSI_SERVER_H__

#include <stdbool.h>
#include <stdint.h> 

#include "access.h"

/* Structure for holding the raw rssi data from each node before it is handled and sent to the rssi client */
typedef struct
{
    uint16_t src_addr;
    int      rssi_sum;
    uint8_t  msg_count;
} raw_rssi_data_entry_t;

/* Object type for rssi server instances. */
typedef struct __rssi_server_t rssi_server_t;

/* Rssi server instance structure */
struct __rssi_server_t
{
    access_model_handle_t            model_handle;     
};

/* Initializes the health server model.
 *
 * @param[in,out] p_server Pointer to the server instance structure.
 * @param[in] element_index Element index to use when registering the health server.
 *
 * @retval NRF_SUCCESS The model was successfully initialized.
 * @retval NRF_ERROR_NULL NULL pointer in function arguments
 *
 * @see access_model_add()
 */
uint32_t rssi_server_init(rssi_server_t * p_server, uint16_t element_index);

/* Initializes the health server model.
 *
 * @param[in] p_server Pointer to the server instance structure.
 * @param[in] element_index Element index to use when registering the health server.
 *
 * @retval NRF_SUCCESS The data was added successfully
 *         NRF_ERROR_NO_MEM No more room for new entries in the data buffer
 *         NRF_ERROR_INTERNAL Overflow on one of the entries message counters
 *         NRF_ERROR_INVALID_PARAM Invalid address parameter
 */
uint32_t rssi_server_add_rssi_data(uint16_t test_address, int8_t rssi);

/** @} */

#endif

