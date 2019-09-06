#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "sync_line.h"
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "nrf_gpio.h"
#include "time_sync_timer.h"
#include "ethernet.h"
#include "socket.h"
#include "w5500.h"
#include "synchronized_timer.h"
#include "toolchain.h"
#include "timer.h"
#include "gpio.h"
#include "ppi.h"
#include "command_system.h"
#include "ethernet.h"
#include "boards.h"
#include "app_timer.h"
#include "log.h"



#define DEBOUNCE_PERIOD_MS 1 

typedef struct{
    sync_sample_package_t sample;
    bool sync_line_state;
} debounce_t;

APP_TIMER_DEF(m_debounce_timer);
static uint32_t m_time_tic;
static bool m_initial_line_state;
static bool m_controls_sync_signal = false;

//TODO: Remove test var
static volatile uint32_t teller;
static volatile uint32_t teller_to;

static void debounce(sync_sample_package_t sample)
{
    teller_to++;

    static debounce_t debounce;
    debounce.sync_line_state = (bool)nrf_gpio_pin_read(SYNC_IN);
    debounce.sample = sample;
    app_timer_start(m_debounce_timer, APP_TIMER_TICKS(DEBOUNCE_PERIOD_MS), &debounce);
}


/* Should trigger each time the sync-line is set high by the master node */
void sync_line_event_handler(void)
{
    uint32_t was_masked;
    _DISABLE_IRQS(was_masked);

    DRIFT_TIMER->TASKS_CAPTURE[0] = 1;
    uint32_t processing_delay = DRIFT_TIMER->CC[0];
    uint32_t now = timer_now() - processing_delay;
    uint32_t adjusted_sync_timer = now - sync_timer_get_offset();

    _ENABLE_IRQS(was_masked);
   
    m_time_tic++;
    sync_sample_package_t sample;
    sample.sample_nr = m_time_tic;
    sample.sample_value = adjusted_sync_timer;
    debounce(sample);
}

void reset_drift_measure_params(void)
{
m_time_tic = 0;
}

void sync_master_set(uint32_t interval)
{
    sync_master_timer_init(interval);
    sync_master_gpio_init();
    sync_master_ppi_init();

    START_SYNC_TIMER = 1;
    m_controls_sync_signal = true;
}

void sync_master_unset(void)
{
    SYNC_TIMER->TASKS_STOP = 1;
    m_controls_sync_signal = false;
}



static void debounce_timeout_handler(void * p_context)
{
    debounce_t* p_debounce = (debounce_t*)p_context;
    bool current_sync_line_state = (bool)nrf_gpio_pin_read(SYNC_IN);

    if(p_debounce->sync_line_state == current_sync_line_state)
    {
        teller++;
        send_over_ethernet((uint8_t*)&p_debounce->sample, CMD_TIME_SYNC);
    }
}



void sync_line_debouncer_init(void)
{
   APP_ERROR_CHECK(app_timer_create(&m_debounce_timer, APP_TIMER_MODE_SINGLE_SHOT, debounce_timeout_handler));
}
