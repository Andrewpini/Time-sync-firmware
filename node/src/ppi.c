#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "timer.h"

/**@brief Function for initialization of PPI.
 */
void ppi_init(void) 
{
    NRF_PPI->CH[PPI_CHANNEL_SYNC_IN].EEP        = (uint32_t) &(NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_SYNC_IN]);
    NRF_PPI->CH[PPI_CHANNEL_SYNC_IN].TEP        = (uint32_t) &(SCAN_TIMESTAMP_TIMER->TASKS_CLEAR);
    NRF_PPI->FORK[PPI_CHANNEL_SYNC_IN].TEP      = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[GPIOTE_CHANNEL_SYNC_LED]);

    NRF_PPI->CH[PPI_CHANNEL_SYNC].EEP        = (uint32_t) &(NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_SYNC_IN]);
    NRF_PPI->CH[PPI_CHANNEL_SYNC].TEP        = (uint32_t) &(NRF_TIMER0->TASKS_CAPTURE[1]);
    NRF_PPI->FORK[PPI_CHANNEL_SYNC].TEP      = (uint32_t) &(NRF_TIMER0->TASKS_START);

    NRF_PPI->CHENSET = (1 << PPI_CHANNEL_SYNC_IN) | (1 << PPI_CHANNEL_SYNC);

}


void sync_master_ppi_init(void){
    // PPI channel configuration for triggering syncing
    NRF_PPI->CH[PPI_CHANNEL_SYNC_OUT].EEP           = (uint32_t) &(SYNC_TIMER->EVENTS_COMPARE[0]);
    NRF_PPI->CH[PPI_CHANNEL_SYNC_OUT].TEP           = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[GPIOTE_CHANNEL_SYNC_OUT]);
    NRF_PPI->CHENSET                                = 1 << PPI_CHANNEL_SYNC_OUT;
}