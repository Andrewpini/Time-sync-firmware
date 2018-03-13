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
    CMD_SERVER_IP_BROADCAST = 10,
    CMD_NEW_SERVER_IP,
    CMD_NEW_FIRMWARE,
    CMD_NEW_ACCESS_ADDRESS
} ctrl_cmd_t;




#endif