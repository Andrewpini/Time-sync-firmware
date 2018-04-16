
#ifndef CONFIG_H
#define CONFIG_H

#define DHCP_TIMER                          NRF_TIMER1
#define SCAN_TIMER                          NRF_TIMER2          // The timer instance to use for capturing timestamps when scanning
#define USE_COUNTER_FROM_ADV                1

#define SUCCESS                             0
#define FAIL                                1

#define LED_HP_CONNECTED_DUTY_CYCLE         0.1
#define LED_HP_ON_DUTY_CYCLE                1.0f
#define LED_HP_OFF_DUTY_CYCLE               0.0f
#define LED_HP_DEFAULT_DUTY_CYCLE           LED_HP_CONNECTED_DUTY_CYCLE

#endif