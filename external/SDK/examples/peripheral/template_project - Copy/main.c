
#include <stdbool.h>
#include <stdint.h>

#include "nrf.h"
#include "boards.h"

#define LED1_PIN                        17
#define LED2_PIN                        18
#define LED3_PIN                        19
#define LED4_PIN                        20

#define TRIGGER_TIMER_INTERVAL_MS       2000

#define TRIGGER_PIN_1                   18
#define TRIGGER_PIN_2                   17
#define TRIGGER_PIN_3                   16
#define TRIGGER_LED                     LED4_PIN

#define TRIGGER_PIN_1_TASK              1
#define TRIGGER_PIN_2_TASK              2
#define TRIGGER_PIN_3_TASK              3
#define TRIGGER_LED_TASK                4

#define PPI_CHANNEL_TRIGGER_1           0
#define PPI_CHANNEL_TRIGGER_2           1


/**@brief Function for initialization oscillators.
 */
void clock_initialization()
{
    /* Start 16 MHz crystal oscillator */
    NRF_CLOCK->EVENTS_HFCLKSTARTED      = 0;
    NRF_CLOCK->TASKS_HFCLKSTART         = 1;

    /* Wait for the external oscillator to start up */
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
    {
        // Do nothing.
    }
}

void gpiote_init(void) 
{
    // GPIOTE configuration for trigger pin 1
    NRF_GPIOTE->CONFIG[TRIGGER_PIN_1_TASK]  = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                            | (TRIGGER_PIN_1 << GPIOTE_CONFIG_PSEL_Pos)
                                            | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                            | (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);

    // GPIOTE configuration for trigger pin 2
    NRF_GPIOTE->CONFIG[TRIGGER_PIN_2_TASK]  = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                            | (TRIGGER_PIN_2 << GPIOTE_CONFIG_PSEL_Pos)
                                            | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                            | (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);

    // GPIOTE configuration for trigger pin 3
    NRF_GPIOTE->CONFIG[TRIGGER_PIN_3_TASK]  = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                            | (TRIGGER_PIN_3 << GPIOTE_CONFIG_PSEL_Pos)
                                            | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                            | (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);
        
    // GPIOTE configuration for LED pin
    NRF_GPIOTE->CONFIG[TRIGGER_LED_TASK]    = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
                                            | (TRIGGER_LED << GPIOTE_CONFIG_PSEL_Pos)
                                            | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
                                            | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);
}

void ppi_init(void) 
{
    // PPI channel for triggering sample syncing
    NRF_PPI->CH[PPI_CHANNEL_TRIGGER_1].EEP      = (uint32_t) &(NRF_TIMER1->EVENTS_COMPARE[0]);
    NRF_PPI->CH[PPI_CHANNEL_TRIGGER_1].TEP      = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[TRIGGER_PIN_1_TASK]);
    NRF_PPI->FORK[PPI_CHANNEL_TRIGGER_1].TEP    = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[TRIGGER_PIN_2_TASK]);
    NRF_PPI->CHENSET                            = 1 << PPI_CHANNEL_TRIGGER_1;

    NRF_PPI->CH[PPI_CHANNEL_TRIGGER_2].EEP      = (uint32_t) &(NRF_TIMER1->EVENTS_COMPARE[0]);
    NRF_PPI->CH[PPI_CHANNEL_TRIGGER_2].TEP      = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[TRIGGER_LED_TASK]);
    NRF_PPI->FORK[PPI_CHANNEL_TRIGGER_2].TEP    = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[TRIGGER_PIN_3_TASK]);
    NRF_PPI->CHENSET                            = 1 << PPI_CHANNEL_TRIGGER_2;
}

void timer_init(void) 
{
    // Timer for sampling of RSSI values
    NRF_TIMER1->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER1->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER1->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER1->CC[0]               = TRIGGER_TIMER_INTERVAL_MS * 1000;                                                         // Compare event every 2 seconds
    NRF_TIMER1->SHORTS              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event
    NRF_TIMER1->INTENSET            = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;               // Enable interrupt for compare event
    NRF_TIMER1->TASKS_START         = 1;

    // Timer for creating timestamps on RSSI capture
    NRF_TIMER2->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER2->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER2->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER2->CC[0]               = 0;
    NRF_TIMER2->TASKS_START         = 1;

    // Timer for sync testing
    NRF_TIMER3->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER3->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER3->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER3->CC[0]               = 0;
    NRF_TIMER3->TASKS_START         = 1;
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;
    nrf_gpio_cfg_output(LED1_PIN);
    nrf_gpio_pin_clear(LED1_PIN);
    clock_initialization();
    timer_init();
    gpiote_init();
    ppi_init();

    while (true)
    {
        // Wait for event
        __WFE();
        
    }
}
/** @} */
