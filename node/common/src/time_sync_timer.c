#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "dhcp.h"
#include "time_sync_timer.h"

void drift_timer_init(void)
{
    DRIFT_TIMER->TASKS_START         = 1;
    DRIFT_TIMER->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    DRIFT_TIMER->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    DRIFT_TIMER->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    DRIFT_TIMER->CC[0]               = UINT32_MAX;                                                                
    DRIFT_TIMER->SHORTS              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event
}


void drift_timer_reset(void)
{
    DRIFT_TIMER->TASKS_STOP = 1;
    DRIFT_TIMER->TASKS_CLEAR = 1;
}

void sync_master_timer_init(uint32_t interval){
    SYNC_TIMER->TASKS_STOP                          = 1;
    SYNC_TIMER->MODE                                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    SYNC_TIMER->BITMODE                             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    SYNC_TIMER->PRESCALER                           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    SYNC_TIMER->CC[0]                               = interval * 1000; 
    SYNC_TIMER->SHORTS                              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event
}


/* When the mesh stack is present we choose to use the implemented timer module instead of a unique timer */
#ifdef MESH_ENABLED
    #include "app_timer.h"
    #include "hal.h"
    #include "mesh_app_utils.h"
    APP_TIMER_DEF(DHCP_TIMER);

void dhcp_timer_handler(void * p_unused){
    DHCP_time_handler();
}

void dhcp_timer_init(void)
{
    ERROR_CHECK(app_timer_create(&DHCP_TIMER, APP_TIMER_MODE_REPEATED, dhcp_timer_handler));
    ERROR_CHECK(app_timer_start(DHCP_TIMER, HAL_MS_TO_RTC_TICKS(1000), NULL));
}

#else
void dhcp_timer_init(void)
{
    DHCP_TIMER->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    DHCP_TIMER->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    DHCP_TIMER->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    DHCP_TIMER->CC[0]               = 1000000;                                                                      // Compare event every second
    DHCP_TIMER->SHORTS              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event
    DHCP_TIMER->INTENSET            = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;               // Enable interrupt for compare event
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_EnableIRQ(TIMER1_IRQn);
 
    DHCP_TIMER->TASKS_START = 1;
}

void TIMER1_IRQHandler(void)
{
    DHCP_TIMER->EVENTS_COMPARE[0] = 0;
    DHCP_time_handler();
}
#endif
