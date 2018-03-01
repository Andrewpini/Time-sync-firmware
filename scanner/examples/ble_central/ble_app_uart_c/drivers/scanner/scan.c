

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "radio.h"
#include "scan.h"

#define RX_BUF_SIZE                                 37
#define MAXIMUM_BYTE_LENGTH_ADV_PACKET              50

static uint8_t crc_counter = 0;

typedef scan_report_t scan_reports_t;
static volatile bool long_packet_error = false;


static uint8_t m_rx_buf[RX_BUF_SIZE];

void scan_init(void)
{
    // Timer for sync testing
    SCAN_TIMER->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    SCAN_TIMER->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    SCAN_TIMER->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    SCAN_TIMER->CC[0]               = 0;
    SCAN_TIMER->TASKS_START         = 1;

}

void scan_get_device_address(uint8_t * source, uint8_t * dest) 
{
    uint8_t i = 0;

    for (i = 0; i < 6; i += 1) 
    {
        dest[i] = source[8 - i];
    }
}

void scan_ble_channel_once(scan_report_t * p_report, uint8_t channel) 
{
    static uint8_t addr[6];
    int16_t rssi;
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

    while (NRF_RADIO->EVENTS_DISABLED == 0) {}
    
    SCAN_TIMER->TASKS_CAPTURE[0] = 1;
    uint32_t capture_time = SCAN_TIMER->CC[0];
    //diff = capture_time - sync_time;

        
    rssi = radio_rssi_get();
    radio_disable();

    scan_get_device_address(m_rx_buf, addr);
    
    report.timestamp = capture_time;
    report.counter += 1;
    report.channel = channel;
    report.rssi = -rssi;
    report.crc_status = NRF_RADIO->CRCSTATUS;
    report.long_packet_error = (long_packet_error) ? 1 : 0;
        
    memcpy((void *) &report.address, addr, 6);
    memcpy((void *) p_report, (const void *) &report, sizeof(scan_report_t));
        
    long_packet_error = false;
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
    scan_report_t report_37;
    scan_report_t report_38;
    scan_report_t report_39;
    

    scan_ble_channel_once(&report_37, ADV_CHANNEL_37);
    memcpy((void *) &p_reports[0], (const void *) &report_37, sizeof(scan_report_t));

    scan_ble_channel_once(&report_38, ADV_CHANNEL_38);
    memcpy((void *) &p_reports[1], (const void *) &report_38, sizeof(scan_report_t));

    scan_ble_channel_once(&report_39, ADV_CHANNEL_39);
    memcpy((void *) &p_reports[2], (const void *) &report_39, sizeof(scan_report_t));
}

//void scan_set_

