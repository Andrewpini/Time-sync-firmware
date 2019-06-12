#ifndef SCAN_H
#define SCAN_H

#include <stdbool.h>
#include <stdint.h>


typedef struct
{
    uint32_t    timestamp;
    uint8_t     id[6];
    uint8_t     channel;
    uint32_t    counter;
    int16_t     rssi;
    uint8_t     address[6];
    uint8_t     crc_status;
    uint8_t     long_packet_error;
} scan_report_t;

void scan_init(void);
uint8_t scan_ble_channel_once(scan_report_t * p_report, uint8_t channel);
void scan_ble_adv_channels_once(scan_report_t * reports);

#endif