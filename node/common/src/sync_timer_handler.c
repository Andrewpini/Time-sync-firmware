#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "time_sync_timer.h"

static int32_t m_offset;

uint32_t sync_timer_get_adjusted_timestamp(void)
{
    DRIFT_TIMER->TASKS_CAPTURE[3] = 1;
    uint32_t timer_val = DRIFT_TIMER->CC[3];
    return (DRIFT_TIMER_MAX + timer_val + m_offset) % DRIFT_TIMER_MAX;
}

uint32_t sync_timer_get_raw_timestamp(void)
{
    DRIFT_TIMER->TASKS_CAPTURE[3] = 1;
    return DRIFT_TIMER->CC[3];

}

int32_t sync_timer_set_timer_offset(int32_t incoming_timestamp)
{
    uint32_t raw_timestamp = sync_timer_get_raw_timestamp();
    m_offset = (DRIFT_TIMER_MAX + incoming_timestamp - raw_timestamp) % DRIFT_TIMER_MAX;
    return m_offset;
}

void sync_timer_increment_timer_offset(int32_t increment)
{
    m_offset += increment;
}
