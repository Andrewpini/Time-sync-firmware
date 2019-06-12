

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "radio.h"
#include "scan.h"
#include "config.h"
#include "util.h"
#include "time_sync_timer.h"

#define RX_BUF_SIZE                                 37
#define MAXIMUM_BYTE_LENGTH_ADV_PACKET              54
#define CHANNEL_37_INDEX                            0
#define CHANNEL_38_INDEX                            1
#define CHANNEL_39_INDEX                            2

typedef scan_report_t scan_reports_t;
static volatile bool long_packet_error = false;
static volatile bool timeout_triggered = false;


static uint8_t crc_counter = 0;
static dict_t channel_dicts[3];

static uint8_t m_rx_buf[RX_BUF_SIZE];

void scan_init(void)
{
    // Timer for sync testing
    SCAN_TIMESTAMP_TIMER->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    SCAN_TIMESTAMP_TIMER->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    SCAN_TIMESTAMP_TIMER->PRESCALER           = 0 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    SCAN_TIMESTAMP_TIMER->CC[0]               = 0;
    SCAN_TIMESTAMP_TIMER->TASKS_START         = 1;

    SCAN_TIMEOUT_TIMER->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    SCAN_TIMEOUT_TIMER->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    SCAN_TIMEOUT_TIMER->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    SCAN_TIMEOUT_TIMER->CC[0]               = 500 * 1000;                                                                   // Event after 100 ms
    //SCAN_TIMEOUT_TIMER->SHORTS              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos
    SCAN_TIMEOUT_TIMER->INTENSET            = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;
    NVIC_ClearPendingIRQ(TIMER3_IRQn);
    NVIC_EnableIRQ(TIMER3_IRQn);

    channel_dicts[CHANNEL_37_INDEX] = dict_new();
    channel_dicts[CHANNEL_38_INDEX] = dict_new();
    channel_dicts[CHANNEL_39_INDEX] = dict_new();
}

void TIMER3_IRQHandler(void) 
{
    SCAN_TIMEOUT_TIMER->EVENTS_COMPARE[0]   = 0;
    timeout_triggered = true;
}


void scan_get_device_address(uint8_t * source, uint8_t * dest) 
{
    uint8_t i = 0;

    for (i = 0; i < 6; i += 1) 
    {
        dest[i] = source[8 - i];
    }
}

uint8_t scan_ble_channel_once(scan_report_t * p_report, uint8_t channel) 
{
    uint8_t addr[6];
    int16_t rssi;
    uint32_t counter = 0;
    scan_report_t report;

    radio_init(channel);

    memset ((void *) m_rx_buf, '\0', RX_BUF_SIZE);
    radio_buffer_configure (&m_rx_buf[0]);
    
    /* BCMATCH config */
    NRF_RADIO->BCC = MAXIMUM_BYTE_LENGTH_ADV_PACKET * 8;

    /* Enable interrupt on BCMATCH events */
    NRF_RADIO->INTENSET = (RADIO_INTENSET_BCMATCH_Msk);

    radio_rx_prepare (true);
    radio_rssi_enable();
    
    SCAN_TIMEOUT_TIMER->TASKS_START = 1;
    SCAN_TIMEOUT_TIMER->TASKS_CLEAR = 1;
    
    while ((NRF_RADIO->EVENTS_DISABLED == 0) && !timeout_triggered) {}

    SCAN_TIMEOUT_TIMER->TASKS_STOP = 1;
    SCAN_TIMEOUT_TIMER->TASKS_CLEAR = 1;

    if (timeout_triggered)
    {
        timeout_triggered = false;
        radio_disable();
        return FAIL;
    }

    SCAN_TIMESTAMP_TIMER->TASKS_CAPTURE[0] = 1;
    uint32_t capture_time = SCAN_TIMESTAMP_TIMER->CC[0];

    rssi = radio_rssi_get();
    radio_disable();

    scan_get_device_address(m_rx_buf, addr);
    

#if USE_COUNTER_FROM_ADV
   counter = *(uint32_t *)&m_rx_buf[9];
#else
    if (channel == ADV_CHANNEL_37)
    {
        counter = dict_find(channel_dicts[0], addr, 0) + 1;
        dict_add(channel_dicts[0], addr, counter);
    }
    else 
    {
        counter = dict_find(channel_dicts[0], addr, 0);
    }
#endif

    report.timestamp = capture_time;
    report.id[0] = (0xB0                         ) & 0xFF;
    report.id[1] = (NRF_FICR->DEVICEADDR[0] >>  8) & 0xFF;
    report.id[2] = (NRF_FICR->DEVICEADDR[0] >> 16) & 0xFF;
    report.id[3] = (NRF_FICR->DEVICEADDR[0] >> 24)       ;
    report.id[4] = (NRF_FICR->DEVICEADDR[1]      ) & 0xFF;
    report.id[5] = (NRF_FICR->DEVICEADDR[1] >>  8) & 0xFF;
    report.counter = counter;
    report.channel = channel;
    report.rssi = -rssi;
    report.crc_status = NRF_RADIO->CRCSTATUS;
    report.long_packet_error = (long_packet_error) ? 1 : 0;
        
    memcpy((void *) &report.address, addr, 6);
    memcpy((void *) p_report, (const void *) &report, sizeof(scan_report_t));
        
    long_packet_error = false;

    return SUCCESS;
}

void RADIO_IRQHandler(void) 
{
    if (NRF_RADIO->EVENTS_ADDRESS)
    {
         NRF_RADIO->EVENTS_ADDRESS = 0;
    }

    if (NRF_RADIO->EVENTS_DISABLED)
    {
         NRF_RADIO->EVENTS_DISABLED = 0;
    }

    if (NRF_RADIO->EVENTS_BCMATCH)
    {
        NRF_RADIO->EVENTS_BCMATCH = 0;
        NRF_RADIO->TASKS_BCSTOP = 1;
        NRF_RADIO->TASKS_DISABLE = 1;
        long_packet_error = true;
    }
}

void scan_ble_adv_channels_once(scan_report_t * p_reports) 
{
    uint8_t err_code = SUCCESS;
    scan_report_t report_37;
    scan_report_t report_38;
    scan_report_t report_39;
    

    err_code = scan_ble_channel_once(&report_37, ADV_CHANNEL_37);
    if (err_code == SUCCESS)
    {   
        memcpy((void *) &p_reports[0], (const void *) &report_37, sizeof(scan_report_t));
    }

    err_code = scan_ble_channel_once(&report_38, ADV_CHANNEL_38);    
    if (err_code == SUCCESS)
    { 
        memcpy((void *) &p_reports[1], (const void *) &report_38, sizeof(scan_report_t));
    }

    err_code = scan_ble_channel_once(&report_39, ADV_CHANNEL_39);
    if (err_code == SUCCESS)
    { 
        memcpy((void *) &p_reports[2], (const void *) &report_39, sizeof(scan_report_t));
    }
}


