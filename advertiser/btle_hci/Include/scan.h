
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t    timestamp;
    uint8_t     channel;
    int16_t     rssi;
    uint8_t     address[6];
    uint8_t     crc_status;
} scan_report_t;

void scan_ble_channel_once(scan_report_t * p_report, uint8_t channel);
void scan_ble_adv_channels_once(scan_report_t * reports);