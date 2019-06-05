/**
 * Copyright (c) 2016 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
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
 * 
 */
#ifndef PCA10056_H
#define PCA10056_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

#define RX_PIN_NUMBER  NRF_GPIO_PIN_MAP(1,10)
#define TX_PIN_NUMBER  NRF_GPIO_PIN_MAP(1,11)
#define CTS_PIN_NUMBER 0xFF
#define RTS_PIN_NUMBER 0xFF
#define HWFC           false

#define BSP_QSPI_SCK_PIN   19
#define BSP_QSPI_CSN_PIN   17
#define BSP_QSPI_IO0_PIN   20
#define BSP_QSPI_IO1_PIN   21
#define BSP_QSPI_IO2_PIN   22
#define BSP_QSPI_IO3_PIN   23

#define SPIM0_SCK_PIN    NRF_GPIO_PIN_MAP(0, 8)  // SPI clock GPIO pin number.
#define SPIM0_MOSI_PIN   NRF_GPIO_PIN_MAP(0, 7)  // SPI Master Out Slave In GPIO pin number.
#define SPIM0_MISO_PIN   NRF_GPIO_PIN_MAP(1, 8)  // SPI Master In Slave Out GPIO pin number.
#define SPIM0_SS_PIN     NRF_GPIO_PIN_MAP(1, 9)  // SPI Slave Select GPIO pin number.



// Low frequency clock source to be used by the SoftDevice
#define NRF_CLOCK_LFCLKSRC      {.source        = NRF_CLOCK_LF_SRC_XTAL,            \
                                 .rc_ctiv       = 0,                                \
                                 .rc_temp_ctiv  = 0,                                \
                                 .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM}

#ifdef __cplusplus
}
#endif

#endif // PCA10056_H
