

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "radio.h"
#include "scan.h"

#define RX_BUF_SIZE                             37

static uint8_t crc_counter = 0;

typedef scan_report_t scan_reports_t;


static uint8_t m_rx_buf[RX_BUF_SIZE];


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
    radio_rx_prepare (true);
    radio_rssi_enable();

    while (NRF_RADIO->EVENTS_DISABLED == 0) {}

    rssi = radio_rssi_get();
    radio_disable();

    scan_get_device_address(m_rx_buf, addr);

    report.channel = channel;
    report.rssi = -rssi;
    report.crc_status = NRF_RADIO->CRCSTATUS;
    memcpy((void *) &report.address, addr, 6);

    memcpy((void *) p_report, (const void *) &report, sizeof(scan_report_t));
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


