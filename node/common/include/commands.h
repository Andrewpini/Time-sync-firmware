#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include <stdint.h>

#define CTRL_CMD_PREFIX                 "CONTROL_COMMAND:"
#define CTRL_CMD_PREFIX_LEN             16
#define CTRL_CMD_CMD_INDEX              CTRL_CMD_PREFIX_LEN
#define CTRL_CMD_PAYLOAD_LEN_INDEX      CTRL_CMD_CMD_INDEX + 1
#define CTRL_CMD_PAYLOAD_INDEX          CTRL_CMD_PAYLOAD_LEN_INDEX + 1

// Typedef for control commands
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
    CMD_TIME_SYNC_STOP              = 0x41
} ctrl_cmd_t;

#endif