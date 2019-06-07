#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "nrf_gpio.h"

/**@brief Function for initialization oscillators.
 */
void clock_init()
{
    if(NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
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
    
    if(NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
    {
        /* Start 32 kHz crystal oscillator */
        NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
        NRF_CLOCK->TASKS_LFCLKSTART    = 1;

        /* Wait for the external oscillator to start up */
        while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
        {
            // Do nothing.
        }
    }
}
