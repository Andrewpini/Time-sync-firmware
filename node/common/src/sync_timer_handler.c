#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "time_sync_timer.h"
#include "rand.h"

static int32_t m_offset;
static uint8_t m_teller_inn = 0;
static volatile uint32_t ran_num;

uint32_t sync_timer_get_adjusted_timestamp(void)
{
//    DRIFT_TIMER->TASKS_CAPTURE[3] = 1;
//    uint32_t timer_val = DRIFT_TIMER->CC[3];
    uint32_t ans = (256 + m_teller_inn + m_offset) % 256;
    prng_t rand;
    ran_num = rand_prng_get(&rand);

    m_teller_inn += (ran_num) %256;
    return ans;
//    return timer_val;
}

uint32_t sync_timer_get_raw_timestamp(void)
{
    DRIFT_TIMER->TASKS_CAPTURE[3] = 1;
    return DRIFT_TIMER->CC[3];

}

int32_t sync_timer_set_timer_offset(int32_t incoming_timestamp)
{
    m_offset = (256 + incoming_timestamp - m_teller_inn) % 256;
    return m_offset;
}

void sync_timer_increment_timer_offset(int32_t increment)
{
    m_offset += increment;
}
