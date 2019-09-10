#ifndef CONFIG_H
#define CONFIG_H

#define LINK_MONITOR_MAX_NEIGHBOR_NODES     32

#define I_AM_ALIVE_SENDING_INTERVAL         2000

#define SYNC_LINE_INTERVAL_MS               1000

/* PWM */
#define PWM_PERIOD_US                       1000

#define LED_HP_CONNECTED_DUTY_CYCLE         0.1f
#define LED_HP_ON_DUTY_CYCLE                1.0f
#define LED_HP_OFF_DUTY_CYCLE               0.0f
#define LED_HP_DEFAULT_DUTY_CYCLE           LED_HP_CONNECTED_DUTY_CYCLE

// GPITOE channelse
#define GPIOTE_CHANNEL_SYNC_IN              0
#define GPIOTE_CHANNEL_SYNC_OUT             1
#define GPIOTE_CHANNEL_SYNC_LED             2
#define GPIOTE_CHANNEL_DFU_BUTTON           3

// PPI channels
#define PPI_CHANNEL_SYNC_IN                 0
#define PPI_CHANNEL_SYNC_OUT                1
#define PPI_CHANNEL_SYNC                    2

#endif