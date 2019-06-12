
#ifndef APP_TIMER_H
#define APP_TIMER_H

#define DRIFT_TIMER_MAX 2000000
#define DRIFT_TIMER_VALUE NRF_TIMER0->CC[1]

#define DRIFT_TIMER                         NRF_TIMER0
#define DHCP_TIMER                          NRF_TIMER1
#define SCAN_TIMESTAMP_TIMER                NRF_TIMER2          // The timer instance to use for capturing timestamps when scanning
#define SCAN_TIMEOUT_TIMER                  NRF_TIMER3
#define SYNC_TIMER                          NRF_TIMER4

#define START_SYNC_TIMER NRF_TIMER4->TASKS_START 


void dhcp_timer_init(void);

void drift_timer_init(void);

void drift_timer_reset(void);

void sync_master_timer_init(uint32_t interval);

#endif