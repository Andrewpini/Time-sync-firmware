#ifndef TIME_SYNC_V1_CONTROLLER_H__
#define TIME_SYNC_V1_CONTROLLER_H__

#include <stdbool.h>
#include <stdint.h> 

#include "access.h"
#include "time_sync_v1_common.h"



/* Object type for rssi server instances. */
typedef struct __time_sync_controller_t time_sync_controller_t;

/** Event callback function type */ 
typedef void (*time_sync_controller_evt_cb_t)(void);

/* Rssi server instance structure */
struct __time_sync_controller_t
{
    access_model_handle_t model_handle;
    time_sync_controller_evt_cb_t time_sync_controller_handler;
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
uint32_t time_sync_controller_init(time_sync_controller_t * p_server, uint16_t element_index, time_sync_controller_evt_cb_t time_sync_controller_handler);

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
void send_timestamp(void);

void sync_set_pub_timer(bool on_off);

/** @} */

#endif

