
#ifndef APP_TIMER_H
#define APP_TIMER_H

#define DRIFT_TIMER_MAX 2000000
#define DRIFT_TIMER_VALUE NRF_TIMER0->CC[1]

void dhcp_timer_init(void);

void drift_timer_init(void);

void drift_timer_reset(void);


#endif