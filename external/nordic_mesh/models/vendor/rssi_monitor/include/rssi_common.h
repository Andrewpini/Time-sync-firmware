#ifndef RSSI_COMMON_H__
#define RSSI_COMMON_H__

/* Model ID for the RSSI Server model. */
#define RSSI_SERVER_MODEL_ID                              0x0005
/* Model ID for the RSSI Client model. */
#define RSSI_CLIENT_MODEL_ID                              0x0006

#ifndef RSSI_DATA_BUFFER_SIZE
#define RSSI_DATA_BUFFER_SIZE 20 /**< This number should reflect the maximum number of nodes that possibly are in direct radio contact with the device */
#endif

/*Packet format of the RSSI data entry sent from the server to the client*/
typedef struct __attribute((packed))
{
    uint16_t src_addr;
    int8_t   mean_rssi;
    uint8_t  msg_count;
} rssi_data_entry_t;

/* RSSI model common opcode */
typedef enum
{
    RSSI_OPCODE_RSSI_ACK                    = 0xC5,
    RSSI_OPCODE_SEND_RSSI_DATA              = 0xC6,
} rssi_common_opcode_t;

#endif
