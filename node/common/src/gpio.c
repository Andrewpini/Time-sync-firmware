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
#include "boards.h"
#include "timer_drift_measurement.h"

#ifdef MESH_ENABLED
#include "ethernet_dfu.h"
#endif

#define DFU_BUTTON_PRESS_FREQUENCY (((uint64_t) (400) << 15ULL) / 1000ULL) // 400 ms

static uint32_t m_last_button_press;

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
                                                | (LED_1 << GPIOTE_CONFIG_PSEL_Pos)
                                                | (0 << GPIOTE_CONFIG_PORT_Pos)
                                                | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                                | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);

    // GPIOTE configuration for DFU button
    NRF_GPIOTE->CONFIG[GPIOTE_CHANNEL_DFU_BUTTON]  = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos)
                                                | (BUTTON_0 << GPIOTE_CONFIG_PSEL_Pos)
                                                | (0 << GPIOTE_CONFIG_PORT_Pos)
                                                | (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos);

    NVIC_EnableIRQ(GPIOTE_IRQn);
    NRF_GPIOTE->INTENSET = (GPIOTE_INTENSET_IN0_Enabled << GPIOTE_INTENSET_IN0_Pos) | (GPIOTE_INTENSET_IN3_Enabled << GPIOTE_INTENSET_IN3_Pos);
    NVIC_SetPriority(GPIOTE_IRQn, 1);
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

    #ifdef MESH_ENABLED
    
    if (NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_DFU_BUTTON]){
        NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_DFU_BUTTON] = 0;

        if(TIMER_DIFF(m_last_button_press, NRF_RTC1->COUNTER) > DFU_BUTTON_PRESS_FREQUENCY){
          m_last_button_press = NRF_RTC1->COUNTER;

          if(get_dfu_flag()){
            dfu_initiate_and_reset();
          }
        }
    }

    #endif
}


void leds_init(void)
{
    nrf_gpio_cfg_output(LED_1);
    nrf_gpio_cfg_output(LED_2);
    nrf_gpio_cfg_output(LED_HP);
    
    nrf_gpio_pin_clear(LED_1);
    nrf_gpio_pin_clear(LED_2);
    nrf_gpio_pin_clear(LED_HP);
}

void button_init_dfu(void)
{
    nrf_gpio_cfg_input(BUTTON_0, BUTTON_PULL);
}

// Initializes time synchronization
void sync_line_init(void) 
{
    nrf_gpio_cfg_input(SYNC_IN, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_output(SYNC_OUT);
    nrf_gpio_pin_clear(SYNC_OUT);
}
