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
#include "sync_line.h"
#include "app_timer.h"
#include "hal.h"
#include "mesh_app_utils.h"

void drift_timer_init(void)
{
    DRIFT_TIMER->TASKS_START         = 1;
    DRIFT_TIMER->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    DRIFT_TIMER->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    DRIFT_TIMER->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    DRIFT_TIMER->TASKS_START         = 1;
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
    SYNC_TIMER->CC[0]                               = interval * 500; 
    SYNC_TIMER->SHORTS                              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event
    SYNC_TIMER->INTENSET                            = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;               // Enable interrupt for compare event

    NVIC_EnableIRQ(TIMER1_IRQn);
    NVIC_SetPriority(TIMER1_IRQn, 7);}

void TIMER1_IRQHandler(void)
{
    SYNC_TIMER->EVENTS_COMPARE[0] = 0;
}
