#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "time_sync_timer.h"
#include "timer.h"

static int32_t m_offset;

uint32_t sync_timer_get_adjusted_timestamp(void)
{
    return timer_now() - m_offset;
}

uint32_t sync_timer_get_raw_timestamp(void)
{
    return timer_now();
}

int32_t sync_timer_set_timer_offset(int32_t offset)
{
    m_offset = offset;
}

void sync_timer_increment_timer_offset(int32_t increment)
{
    m_offset = increment + m_offset;
}

int32_t sync_timer_get_current_offset(void)
{
    return m_offset;
}

void sync_timer_reset(void)
{
    m_offset = 0;
}
