#ifndef HEALTH_UTIL_H__
#define HEALTH_UTIL_H__

#include <stdbool.h>
#include <stdint.h>

#include "access.h"

/* Model ID for the RSSI Util model. */
#define RSSI_UTIL_MODEL_ID                                0x0007

/** Object type for rssi util instances. */
typedef struct __rssi_util_t rssi_util_t;

/* Rssi util instance structure */
struct __rssi_util_t
{
    access_model_handle_t            model_handle;
};

/* Rssi model opcodes. */
typedef enum
{
    RSSI_OPCODE_REQUEST_DATABASE_BEACON     = 0xC7,
    RSSI_OPCODE_SEND_DATABASE_BEACON        = 0xC8,
} rssi_util_opcode_t;

/* Initializes the rssi util model.
 *
 * @param[in,out] p_util Pointer to the rssi util instance structure.
 * @param[in] element_index Element index to use when registering the health util.
 *
 * @retval NRF_SUCCESS The model was successfully initialized.
 *
 * @see access_model_add()
 */
uint32_t rssi_util_init(rssi_util_t * p_util);

/* Searches for a corresponding element address to a ingoing mac address. Starts the process of gathering 
 * mac/element address pairs from nearby nodes if the corresponding element address is not found 
 *
 * @param[in] p_util Pointer to the rssi util instance structure.
 * @param[in] p_mac_addr pointer to the ingoing mac address.
 * @param[in/out] p_element_addr pointer to where the corresponding element address is to be stored.
 *
 * @retval NRF_SUCCESS A corresponding element address was found.
 * @retval NRF_ERROR_NULL NULL pointer in function arguments
 * @retval NRF_ERROR_NOT_FOUND A corresponding element address was not found.
 *
 */
uint32_t rssi_util_mac_to_element_addr_find(rssi_util_t * p_util, const uint8_t* p_mac_addr, dsm_local_unicast_address_t* p_element_addr);

/** @} */

#endif
