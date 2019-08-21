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
#include "time_sync_handler.h"
#include "toolchain.h"
#include "timer.h"
#include "gpio.h"
#include "ppi.h"
#include "command_system.h"
#include "ethernet.h"

static uint32_t m_time_tic;
static bool m_controls_sync_signal = false;

static void send_drift_timing_sample(uint32_t adjusted_sync_timer) // TODO: Need to build packets in a better way
{
    uint8_t buf[SCAN_REPORT_LENGTH];
//    uint8_t len = 0;
    uint8_t own_MAC[6] = {0};
    get_own_MAC(own_MAC);

    sprintf((char *)&buf[0], "{ \"nodeID\" : \"%02x:%02x:%02x:%02x:%02x:%02x\", \"drift\" : %d, \"timetic\" : %d}", 
                    own_MAC[0], own_MAC[1], own_MAC[2], own_MAC[3], own_MAC[4], own_MAC[5],
                    adjusted_sync_timer,
                    m_time_tic);

//    len = strlen((const char *)&buf[0]);

//    command_system_package_t package;
//
//    package.
    sync_sample_package_t sample;
    sample.sample_nr = m_time_tic;
    sample.sample_value = adjusted_sync_timer;
    send_over_ethernet((uint8_t*)&sample, CMD_TIME_SYNC);
}

// Should triggers each time the sync-line is set high by the master node
void sync_line_event_handler(void)
{
    m_time_tic++;

    uint32_t was_masked;
    _DISABLE_IRQS(was_masked);

    DRIFT_TIMER->TASKS_CAPTURE[0] = 1;
    uint32_t processing_delay = DRIFT_TIMER->CC[0];
    uint32_t now = timer_now() - processing_delay;
    uint32_t adjusted_sync_timer = now - sync_timer_get_offset();

    _ENABLE_IRQS(was_masked);

    send_drift_timing_sample(adjusted_sync_timer);
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