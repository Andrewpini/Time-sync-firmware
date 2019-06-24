#ifndef TIME_SYNC_V1_COMMON_H__
#define TIME_SYNC_V1_COMMON_H__

/* Model ID for the time sync controller model. */
#define TIME_SYNC_CONTROLLER_MODEL_ID 0x0026

/* Time sync model common opcode */
typedef enum
{
    TIME_SYNC_OPCODE_SEND_OWN_TIMESTAMP = 0xF5,
    TIME_SYNC_OPCODE_ACK = 0xF6
} time_sync_common_opcode_t;

#endif
