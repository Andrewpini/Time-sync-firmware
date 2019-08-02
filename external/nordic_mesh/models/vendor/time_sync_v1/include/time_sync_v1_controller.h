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

#ifndef TIME_SYNC_V1_CONTROLLER_H__
#define TIME_SYNC_V1_CONTROLLER_H__

#include <stdbool.h>
#include <stdint.h> 

#include "access.h"
#include "time_sync_v1_common.h"

/**
 * @defgroup TIME_SYNC_CONTROLLER Time Sync controller
 * @ingroup TIME_SYNC_COMMON
 * Model implementing time sync controller model.
 * @{
 */

/* Object type for rssi server instances. */
typedef struct __time_sync_controller_t time_sync_controller_t;

/* Time sync controller instance structure */
struct __time_sync_controller_t
{
    access_model_handle_t model_handle;
};

/* Initializes the time sync controller model.
 *
 * @param[in,out] p_controller Pointer to the controller instance structure.
 * @param[in] element_index Element index to use when registering the time sync controller.
 *
 * @retval NRF_SUCCESS The model was successfully initialized.
 * @retval NRF_ERROR_NULL NULL pointer in function arguments
 *
 * @see access_model_add()
 */
uint32_t time_sync_controller_init(time_sync_controller_t * p_controller, uint16_t element_index);

/**
 * Starts a syncronization session with the caller of this function as the root device.
 */
void time_sync_controller_synchronize(time_sync_controller_t * p_controller);

/**
 * Resets the time sync controller on all nodes.
 *
 * @note Should always be used before switching root time sync device.
 *
 * @param[in] repeat The number of reset messages to send. should reflect connection quality in the
 * @note  Should reflect the connection quality in the network to ensure that all devices recieves 
 *        the reset message.
 */
void time_sync_controller_reset(time_sync_controller_t * p_controller, uint8_t repeat);

/* For development only */
void sync_set_pub_timer(bool on_off);

/** @} end of TIME_SYNC_CONTROLLER */

#endif

