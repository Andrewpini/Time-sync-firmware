
#ifndef CMD_SYSTEM_H
#define CMD_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#include "rssi_common.h"
#include "config.h"

/* Typedef for control commands */
typedef enum
{
    CMD_RESET_ALL_NODES             = 0x01,
    CMD_RESET_NODE_MAC              = 0x02,
    CMD_DFU_ALL                     = 0x10,
    CMD_DFU_MAC                     = 0x11,
    CMD_DFU_BUTTON_ENABLE           = 0x12,
    CMD_DFU_BUTTON_DISABLE          = 0x13,
    CMD_ALL_HPLED_ON                = 0x20,
    CMD_ALL_HPLED_OFF               = 0x21,
    CMD_SINGLE_HPLED_ON             = 0x22,
    CMD_SINGLE_HPLED_OFF            = 0x23,
    CMD_SYNC_LINE_START_MASTER      = 0x30,
    CMD_SYNC_LINE_RESET             = 0x31,
    CMD_SYNC_LINE_STOP              = 0x32,
    CMD_TIME_SYNC_START_MASTER      = 0x40,
    CMD_TIME_SYNC_STOP              = 0x41,
    CMD_I_AM_ALIVE                  = 0x50,
    CMD_COMMAND                     = 0x51,
    CMD_ACK                         = 0x52,
    CMD_LINK_MONITOR                = 0x53,
    CMD_TIME_SYNC                   = 0x54
} ctrl_cmd_t;

typedef struct __attribute((packed))
{
    uint32_t identifier;
    uint16_t element_address;
    uint8_t number_of_entries;
    rssi_data_entry_t rssi_data_entry[LINK_MONITOR_MAX_NEIGHBOR_NODES];
} link_monitor_package_t;

typedef struct __attribute((packed))
{
    uint8_t ip[4];
    uint16_t element_address;
} i_am_alive_package_t;

typedef struct __attribute((packed))
{
    uint8_t target_mac[6];
    uint32_t tid;
} hp_led_package_t;

typedef struct __attribute((packed))
{
    uint8_t mac[6];
    uint32_t tid;
} ack_package_t;

typedef struct __attribute((packed))
{
    uint32_t m_time_tic;
    uint32_t adjusted_sync_timer;
} sync_line_package_t;

typedef struct __attribute((packed))
{
    uint32_t identifier;
    ctrl_cmd_t opcode;
    uint8_t mac[6];
    union{
      i_am_alive_package_t i_am_alive_package;
      ack_package_t ack_package;
      hp_led_package_t hp_led_package;
      sync_line_package_t sync_line_package;
    } payload;
} command_system_package_t;

void check_ctrl_cmd(void);

#endif
