#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "nrf_gpio.h"
#include "gpio.h"
#include "timer_drift_measurement.h"


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

void sync_master_gpio_init(void){
    nrf_gpio_cfg_output(SYNC_IN);
    // GPIOTE configuration for syncing of clocks
    NRF_GPIOTE->CONFIG[GPIOTE_CHANNEL_SYNC_OUT]     = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                                    | (SYNC_IN << GPIOTE_CONFIG_PSEL_Pos)
                                                    | (0 << GPIOTE_CONFIG_PORT_Pos)
                                                    | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                                    | (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);
}

/*Triggers each time the sync-line is set high by the master node*/
void GPIOTE_IRQHandler(void)
{
    if (NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_SYNC_IN]){
        NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_SYNC_IN] = 0;
        sync_line_event_handler();
    }
}


void leds_init(void)
{
    nrf_gpio_cfg_output(LED_0);
    nrf_gpio_cfg_output(LED_1);
    nrf_gpio_cfg_output(LED_HP);
    
    nrf_gpio_pin_clear(LED_1);
    nrf_gpio_pin_clear(LED_HP);
}


// Initializes time synchronization
void sync_line_init(void) 
{
    nrf_gpio_cfg_input(SYNC_IN, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_output(SYNC_OUT);
    nrf_gpio_pin_clear(SYNC_OUT);
}
