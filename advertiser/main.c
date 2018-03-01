/**
 * Copyright (c) 2009 - 2017, Nordic Semiconductor ASA
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

/** @file
* @brief Example template project.
* @defgroup nrf_templates_example Example Template
*
*/

#include <stdbool.h>
#include <stdint.h>

#include "nrf.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_delay.h"

#include "scan.h"
#include "advertise.h"

#define PRINT_SCAN_REPORTS              1
#define PRINT_SCAN_GRAPH_DATA           0

#define LED2_PIN                        14
#define LED4_PIN                        16
#define SYNC_PIN                        31
#define PPI_CHANNEL_LED                 0
#define PPI_CHANNEL_SYNC                1
#define SCAN_TIMER_MS                   500


static volatile uint32_t flag           = 0;
static volatile uint32_t sync_time      = 0;
static volatile uint32_t capture_time   = 0;
static volatile uint32_t diff           = 0;


void print_scan_reports(scan_report_t * reports)
{
    uint8_t i;
   
    for (i = 0; i < 3; i += 1)
    {
#if PRINT_SCAN_REPORTS
        NRF_LOG_RAW_INFO("Time since last sync: %d\t ", diff);
        NRF_LOG_RAW_INFO("Advertisement received:\t %02x:%02x:%02x:%02x:%02x:%02x \t", 
                            reports[i].address[0], reports[i].address[1], reports[i].address[2], 
                            reports[i].address[3], reports[i].address[4], reports[i].address[5]);
        NRF_LOG_RAW_INFO("RSSI: %d dBm \t\t", reports[i].rssi);
        NRF_LOG_RAW_INFO("Channel: %d\t\t", reports[i].channel);
        NRF_LOG_RAW_INFO("CRC: %01d\r\n", reports[i].crc_status);
#endif
#if PRINT_SCAN_GRAPH_DATA
        NRF_LOG_RAW_INFO("%d ", reports[i].rssi);
#endif
    }
}

/**@brief Function for initialization oscillators.
 */
void clock_initialization()
{
    /* Start 16 MHz crystal oscillator */
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART    = 1;

    /* Wait for the external oscillator to start up */
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
    {
        // Do nothing.
    }
}

void log_init() 
{
    uint32_t err_code;
    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


void gpiote_init(void) 
{
    // GPIOTE configuration for LED pin
    NRF_GPIOTE->CONFIG[0]           = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                    | (LED4_PIN << GPIOTE_CONFIG_PSEL_Pos)
                                    | (0 << GPIOTE_CONFIG_PORT_Pos)
                                    | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                    | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);

    // GPIOTE configuration for syncing of clocks
    NRF_GPIOTE->CONFIG[1]           = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos)
                                    | (SYNC_PIN << GPIOTE_CONFIG_PSEL_Pos)
                                    | (0 << GPIOTE_CONFIG_PORT_Pos)
                                    | (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);

    // GPIOTE configuration for LED pin
    NRF_GPIOTE->CONFIG[2]           = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                    | (LED2_PIN << GPIOTE_CONFIG_PSEL_Pos)
                                    | (0 << GPIOTE_CONFIG_PORT_Pos)
                                    | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                    | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);

    NRF_GPIOTE->INTENSET            = GPIOTE_INTENSET_IN1_Set << GPIOTE_INTENSET_IN1_Pos;
    NVIC_ClearPendingIRQ(GPIOTE_IRQn);
    NVIC_EnableIRQ(GPIOTE_IRQn);

}

void ppi_init(void) 
{
    // PPI channel for toggling LED on timer compare event
    NRF_PPI->CH[PPI_CHANNEL_LED].EEP        = (uint32_t) &(NRF_TIMER1->EVENTS_COMPARE[0]);
    NRF_PPI->CH[PPI_CHANNEL_LED].TEP        = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[0]);
    NRF_PPI->FORK[PPI_CHANNEL_LED].TEP      = (uint32_t) &(NRF_TIMER2->TASKS_CAPTURE[0]);
    NRF_PPI->CHENSET                        = 1 << PPI_CHANNEL_LED;

    // PPI channel for syncing sampling clocks
    NRF_PPI->CH[PPI_CHANNEL_SYNC].EEP       = (uint32_t) &(NRF_GPIOTE->EVENTS_IN[1]);
    NRF_PPI->CH[PPI_CHANNEL_SYNC].TEP       = (uint32_t) &(NRF_TIMER3->TASKS_CAPTURE[0]);
    NRF_PPI->FORK[PPI_CHANNEL_SYNC].TEP     = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[2]);
    NRF_PPI->CHENSET                        = 1 << PPI_CHANNEL_SYNC;

}

void timer_init(void) 
{
    // Timer for sampling of RSSI values
    NRF_TIMER1->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER1->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER1->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER1->CC[0]               = SCAN_TIMER_MS * 1000;                                                         // Compare event every 2 seconds
    NRF_TIMER1->SHORTS              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event
    NRF_TIMER1->INTENSET            = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;               // Enable interrupt for compare event
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_EnableIRQ(TIMER1_IRQn);

    // Timer for creating timestamps on RSSI capture
    NRF_TIMER2->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER2->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER2->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER2->CC[0]               = 0;

    // Timer for sync testing
    NRF_TIMER3->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER3->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER3->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER3->CC[0]               = 0;


    NRF_TIMER1->TASKS_START         = 1;
    NRF_TIMER2->TASKS_START         = 1;
    NRF_TIMER3->TASKS_START         = 1;
}

void TIMER1_IRQHandler(void) 
{
    NRF_TIMER1->EVENTS_COMPARE[0]   = 0;
    flag = 1;
}

void calc_time_since_sync(void)
{
    NRF_TIMER2->TASKS_CAPTURE[0] = 1;
    capture_time = NRF_TIMER2->CC[0];
    diff = capture_time - sync_time;
}

void GPIOTE_IRQHandler(void)
{
    NRF_GPIOTE->EVENTS_IN[1] = 0;
    sync_time = NRF_TIMER3->CC[0];
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;
    scan_report_t scan_reports[3];
    uint32_t counter = 1;
    
    clock_initialization();
    timer_init();
    gpiote_init();
    ppi_init();
    
    log_init();
    advertise_init();

    NRF_LOG_INFO("Started");
    NRF_LOG_FLUSH();
    while (true)
    {
        if (flag == 1) {
            flag = 0;
            
            advertise_set_payload((uint8_t *)&counter, sizeof(counter));
            advertise_ble_channel_once(37);   
            advertise_ble_channel_once(38);    
            advertise_ble_channel_once(39);
            counter++;

        }
    }
}
/** @} */
