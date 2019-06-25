#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"

static int32_t m_offset;

uint32_t sync_timer_get_adjusted_timestamp(void)
{
    NRF_TIMER3->TASKS_CAPTURE[3] = 1;
    uint32_t timer_val = NRF_TIMER3->CC[3];
    return timer_val + m_offset;
}

uint32_t sync_timer_get_raw_timestamp(void)
{
    NRF_TIMER3->TASKS_CAPTURE[3] = 1;
    return NRF_TIMER3->CC[3];

}

void sync_timer_set_timer_offset(int32_t incoming_timestamp)
{
  
    m_offset = incoming_timestamp - sync_timer_get_raw_timestamp();
}

void sync_timer_increment_timer_offset(int32_t increment)
{
    m_offset += increment;
}
