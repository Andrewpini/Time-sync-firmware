
#ifndef CONFIG_H
#define CONFIG_H

#define USE_COUNTER_FROM_ADV                1

#define SUCCESS                             0
#define FAIL                                1

#define TX_POWER                            RADIO_TXPOWER_TXPOWER_0dBm

#define ADVERTISING_INTERVAL                300
#define SYNC_INTERVAL_MS                    1000

#define LED_HP_CONNECTED_DUTY_CYCLE         0.1f
#define LED_HP_ON_DUTY_CYCLE                1.0f
#define LED_HP_OFF_DUTY_CYCLE               0.0f
#define LED_HP_DEFAULT_DUTY_CYCLE           LED_HP_CONNECTED_DUTY_CYCLE

#define I_AM_ALIVE_SENDING_INTERVAL         2000

// GPITOE channelse
#define GPIOTE_CHANNEL_SYNC_IN              0
#define GPIOTE_CHANNEL_SYNC_OUT             1
#define GPIOTE_CHANNEL_SYNC_LED             2
#define GPIOTE_CHANNEL_DFU_BUTTON           3

// PPI channels
#define PPI_CHANNEL_SYNC_IN                 0
#define PPI_CHANNEL_SYNC_OUT                1
#define PPI_CHANNEL_SYNC                    2

// 
#define SCAN_REPORT_LENGTH                  200
#define SERVER_IP_PREFIX                    "position_server: "
#define SERVER_IP_PREFIX_LEN                17

#endif