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

#ifndef SYNC_TIMER_HANDLER_H
#define SYNC_TIMER_HANDLER_H

/**
 * @defgroup SYNC_TIMER_HANDLER The handler for the time syncronization
 * @ingroup TIME_SYNC_COMMON
 * Handler responsible for keeping track of and adjusting the time offset
 * that ensures time sync between devices
 * @{
 */

/**
 * Gets the adjusted timestamp value of the timer.
 *
 * @retval The adjusted timestamp value.
 */
uint32_t sync_timer_get_adjusted_timestamp(void);

/**
 * Gets the raw timestamp value of the timer (without offset).
 *
 * @retval The raw timestamp value.
 */
uint32_t sync_timer_get_raw_timestamp(void);

/**
 * Set the time sync offset.
 *
 * @param[in] offset    The value of the offset.
 */
void sync_timer_set_timer_offset(int32_t offset);

/**
 * Increment the time sync offset.
 *
 * @param[in] increment    The value of the increment.
 */
void sync_timer_increment_timer_offset(int32_t increment);

/**
 * Reset the time sync offset.
 */
void sync_timer_reset(void);

/**
 * Get the time sync offset.
 *
 * @retval The current offset value.
 */
int32_t sync_timer_get_offset(void);

/** @} end of SYNC_TIMER_HANDLER */

#endif