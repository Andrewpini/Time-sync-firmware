
#ifndef APP_TIMER_H
#define APP_TIMER_H

#define DRIFT_TIMER_MAX 1000000

#define DRIFT_TIMER                         NRF_TIMER3
#define SYNC_TIMER                          NRF_TIMER1
  
#define DRIFT_TIMER_VALUE DRIFT_TIMER->CC[1]
#define START_SYNC_TIMER SYNC_TIMER->TASKS_START 


void dhcp_timer_init(void);

void drift_timer_init(void);

void drift_timer_reset(void);

void sync_master_timer_init(uint32_t interval);

#endif