#ifndef ETHERNET_H
#define ETHERNET_H

#include "command_system.h"

#define SOCKET_DHCP                         2
#define SOCKET_COMMAND                      3
#define SOCKET_LINK_MONITOR                 4
#define SOCKET_TIME_SYNC                    5
#define SOCKET_RX                           6
#define TX_IP                               ((const uint8_t[]){255, 255, 255, 255})
#define COMMAND_RX_PORT                     10000
#define COMMAND_TX_PORT                     11001
#define LINK_MONITOR_PORT                   11002
#define TIME_SYNC_PORT                      11003
#define TX_FLAGS                            0x00
#define TX_BUF_SIZE                         2048

#define BSP_QSPI_SCK_PIN   19
#define BSP_QSPI_CSN_PIN   17
#define BSP_QSPI_IO0_PIN   20
#define BSP_QSPI_IO1_PIN   21
#define BSP_QSPI_IO2_PIN   22
#define BSP_QSPI_IO3_PIN   23

#define SPIM0_SCK_PIN    NRF_GPIO_PIN_MAP(0, 8)  // SPI clock GPIO pin number.
#define SPIM0_MOSI_PIN   NRF_GPIO_PIN_MAP(0, 7)  // SPI Master Out Slave In GPIO pin number.
#define SPIM0_MISO_PIN   NRF_GPIO_PIN_MAP(1, 8)  // SPI Master In Slave Out GPIO pin number.
#define SPIM0_SS_PIN     NRF_GPIO_PIN_MAP(1, 9)  // SPI Slave Select GPIO pin number.

#define SPI_TIMER_TIMEOUT_INTERVAL_MS           1000

/* SPI master module number */
typedef enum
{
    SPI0 = 0,               /*!< SPI module 0 */
    SPI1                    /*!< SPI module 1 */
} SPIModuleNumber;

void dhcp_init(void);

void ethernet_init(void);

void send_over_ethernet(uint8_t* payload_package, ctrl_cmd_t msg_opcode);

void get_own_ip(uint8_t* p_IP);

void get_own_mac(uint8_t* p_MAC);

#endif
