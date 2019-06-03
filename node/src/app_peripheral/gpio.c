#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "gpio.h"

/**@brief Function for initialization of GPIOTE
 */
void gpiote_init(void) 
{
    // GPIOTE configuration for syncing of clocks
    NRF_GPIOTE->CONFIG[GPIOTE_CHANNEL_SYNC_IN]  = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos)
                                                | (SYNC_IN << GPIOTE_CONFIG_PSEL_Pos)
                                                | (0 << GPIOTE_CONFIG_PORT_Pos)
                                                | (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);

    // GPIOTE configuration for LED pin
    NRF_GPIOTE->CONFIG[GPIOTE_CHANNEL_SYNC_LED] = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                                | (LED_0 << GPIOTE_CONFIG_PSEL_Pos)
                                                | (0 << GPIOTE_CONFIG_PORT_Pos)
                                                | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                                | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);

    NVIC_EnableIRQ(GPIOTE_IRQn);
    NRF_GPIOTE->INTENSET = (GPIOTE_INTENSET_IN0_Enabled << GPIOTE_INTENSET_IN0_Pos);
//    NVIC_SetPriority(GPIOTE_IRQn, 1);
}