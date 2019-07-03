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
    WHOAMI_START                    = 1,
    WHOAMI_STOP                     = 2,
    CMD_SERVER_IP_BROADCAST         = 10,
    CMD_NEW_SERVER_IP               = 12,
    CMD_NEW_ACCESS_ADDRESS          = 13,
    CMD_ADVERTISING_START           = 14,
    CMD_ADVERTISING_STOP            = 15,
    CMD_ALL_HPLED_ON                = 40,
    CMD_ALL_HPLED_OFF               = 41,
    CMD_ALL_HPLED_DEFAULT           = 42,
    CMD_ALL_HPLED_NEW_DEFAULT       = 43,
    CMD_ALL_HPLED_CUSTOM            = 44,
    CMD_SINGLE_HPLED_ON             = 50,
    CMD_SINGLE_HPLED_OFF            = 51,
    CMD_SINGLE_HPLED_DEFAULT        = 52,
    CMD_SINGLE_HPLED_CUSTOM         = 53,
    CMD_SINGLE_ADVERTISING_ON       = 60,
    CMD_SINGLE_ADVERTISING_OFF      = 61,
    CMD_SYNC_NODE_SET               = 70,
    CMD_SYNC_SET_INTERVAL           = 71,
    CMD_SYNC_RESET                  = 72,
    CMD_NEW_FIRMWARE_ALL            = 80,
    CMD_NEW_FIRMWARE_MAC            = 81,
    CMD_NEW_FIRMWARE_BUTTON_ENABLE  = 82,
    CMD_NEW_FIRMWARE_BUTTON_DISABLE = 83,
    RESET_SYNC                      = 3
} ctrl_cmd_t;




#endif