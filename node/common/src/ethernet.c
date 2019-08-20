#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ethernet.h"
#include "app_error.h"
#include "log.h"
#include "hal.h"
#include "mesh_app_utils.h"
#include "socket.h"
#include "app_timer.h"
#include "nrf_delay.h"
#include "dhcp.h"
#include "dhcp_cb.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "command_system.h"

static uint8_t TX_BUF[TX_BUF_SIZE];
static uint8_t own_MAC[6]          = {0};
static uint8_t own_IP[4]           = {0};
static uint8_t critical_section_depth;

APP_TIMER_DEF(DHCP_TIMER);

#define SPI_INSTANCE    0 /**< SPI instance index. */
static const            nrf_drv_spi_t spi_inst = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);    /**< SPI instance. */

static wiz_NetInfo gWIZNETINFO = 
{
    .ip     = {1, 1, 1, 1},
    .mac    = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
    .sn     = {255, 255, 255, 0},
    .gw     = {10, 0, 0, 1}, 
    .dns    = {8, 8, 8, 8},
    .dhcp   = NETINFO_DHCP 
};

static void print_network_info(void)
{
    uint8_t tmpstr[6];
  
    ctlnetwork(CN_GET_NETINFO, (void*)&gWIZNETINFO);

    ctlwizchip(CW_GET_ID,(void*)tmpstr);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "=== %s NET CONF ===\r\n",(char*)tmpstr);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "MAC:\t %02X:%02X:%02X:%02X:%02X:%02X\r\n",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],
              gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "SIP:\t %d.%d.%d.%d\r\n", gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "GAR:\t %d.%d.%d.%d\r\n", gWIZNETINFO.gw[0],gWIZNETINFO.gw[1],gWIZNETINFO.gw[2],gWIZNETINFO.gw[3]);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "SUB:\t %d.%d.%d.%d\r\n", gWIZNETINFO.sn[0],gWIZNETINFO.sn[1],gWIZNETINFO.sn[2],gWIZNETINFO.sn[3]);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "DNS:\t %d.%d.%d.%d\r\n", gWIZNETINFO.dns[0],gWIZNETINFO.dns[1],gWIZNETINFO.dns[2],gWIZNETINFO.dns[3]);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "======================\r\n");
}

static void critical_section_enter(void)
{
  uint8_t dummy;
  app_util_critical_region_enter(&dummy);
  critical_section_depth++;
}

static void critical_section_exit(void)
{
  critical_section_depth--;
  app_util_critical_region_exit(critical_section_depth);
}

static bool spi_master_tx(SPIModuleNumber spi_num, uint16_t transfer_size, const uint8_t *tx_data)
{
    if(tx_data == 0)
    {
        return false;
    }
    
    if(nrf_drv_spi_transfer(&spi_inst, tx_data, 1, NULL, 0) != NRF_SUCCESS)
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "SPI failed\r\n"); 
        return false;
    }
    
    return true;
}

static bool spi_master_rx(SPIModuleNumber spi_num, uint16_t transfer_size, uint8_t *rx_data)
{    
     if(rx_data == 0)
    {
        return false;
    }
    
    if(nrf_drv_spi_transfer(&spi_inst, NULL, 0, rx_data, 1) != NRF_SUCCESS)
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "SPI failed\r\n"); 
        return false;
    }
    
    return true;
}

static void wizchip_select(void)
{
    nrf_gpio_pin_clear(SPIM0_SS_PIN);
}

static void wizchip_deselect(void)
{
    nrf_gpio_pin_set(SPIM0_SS_PIN);
}

static uint8_t wizchip_read(void)
{
    uint8_t recv_data;

    spi_master_rx(SPI0,1,&recv_data);

    return recv_data;
}

static void wizchip_write(uint8_t wb)
{
    spi_master_tx(SPI0, 1, &wb);
}

static void command_socket_init(void) 
{
    socket(SOCKET_COMMAND, Sn_MR_UDP, COMMAND_PORT, TX_FLAGS); 
}

static void link_monitor_socket_init(void) 
{
    socket(SOCKET_LINK_MONITOR, Sn_MR_UDP, LINK_MONITOR_PORT, TX_FLAGS); 
}

static void time_sync_socket_init(void) 
{
    socket(SOCKET_TIME_SYNC, Sn_MR_UDP, TIME_SYNC_PORT, TX_FLAGS); 
}

static void rx_socket_init(void)
{
    socket(SOCKET_RX, Sn_MR_UDP, BROADCAST_PORT, SF_IO_NONBLOCK);
}

static void ethernet_spi_init(void)
{
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.sck_pin   = SPIM0_SCK_PIN;
    spi_config.mosi_pin  = SPIM0_MOSI_PIN;
    spi_config.miso_pin  = SPIM0_MISO_PIN;
    spi_config.ss_pin    = NRF_DRV_SPI_PIN_NOT_USED;
    spi_config.orc       = 0x0;
    spi_config.mode      = NRF_DRV_SPI_MODE_3;
    spi_config.frequency = NRF_DRV_SPI_FREQ_1M;
    nrf_gpio_cfg_output(SPIM0_SS_PIN); /* For manual controlling CS Pin */
    nrf_gpio_pin_clear(SPIM0_SS_PIN);

    uint32_t err_code = nrf_drv_spi_init(&spi_inst, &spi_config, NULL, NULL);
    APP_ERROR_CHECK(err_code);
}

static void dhcp_timer_handler(void * p_unused){
    DHCP_time_handler();
}

static void dhcp_timer_init(void)
{
    ERROR_CHECK(app_timer_create(&DHCP_TIMER, APP_TIMER_MODE_REPEATED, dhcp_timer_handler));
    ERROR_CHECK(app_timer_start(DHCP_TIMER, HAL_MS_TO_RTC_TICKS(1000), NULL));
}

static void ethernet_dhcp_init(void)
{
    uint32_t ret;
    uint8_t dhcp_retry = 0;

    dhcp_timer_init();
    DHCP_init(SOCKET_DHCP, TX_BUF);
    reg_dhcp_cbfunc(w5500_dhcp_assign, w5500_dhcp_assign, w5500_dhcp_conflict);

    while(1)
    {
        ret = DHCP_run();

        if(ret == DHCP_IP_LEASED)
        {
            getSHAR(&own_MAC[0]);
            getIPfromDHCP(&own_IP[0]);
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "This device' IP: %d.%d.%d.%d\r\n", own_IP[0], own_IP[1], own_IP[2], own_IP[3]);
            print_network_info();
            break;
        }
        else if(ret == DHCP_FAILED)
        {
             dhcp_retry++;  
        }

        if(dhcp_retry > 10)
        {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Initialization of DHCP failed\r\n");
            while(1)
            {
            }
        }
    }
}

/********** Interface functions **********/

void ethernet_init(void)
{
    uint8_t tmp;
    int8_t error_code;
    uint8_t memsize[2][8] = {{2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}};
    wiz_NetTimeout timeout_info;

    /* Initializing SPI communication with the ethernet (Wiznet W5500) chip */   
    ethernet_spi_init();

    /* Using Nordic chip factory information as MAC address */       
    gWIZNETINFO.mac[0] = (0xB0                         ) & 0xFF;
    gWIZNETINFO.mac[1] = (NRF_FICR->DEVICEADDR[0] >>  8) & 0xFF;
    gWIZNETINFO.mac[2] = (NRF_FICR->DEVICEADDR[0] >> 16) & 0xFF;
    gWIZNETINFO.mac[3] = (NRF_FICR->DEVICEADDR[0] >> 24)       ;
    gWIZNETINFO.mac[4] = (NRF_FICR->DEVICEADDR[1]      ) & 0xFF;
    gWIZNETINFO.mac[5] = (NRF_FICR->DEVICEADDR[1] >>  8) & 0xFF;

    /* Registering necessary callback functions */
    reg_wizchip_cris_cbfunc(critical_section_enter, critical_section_exit);
    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
    reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);

    /* WIZCHIP SOCKET Buffer initialize */	
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "W5500 memory init\r\n");
    error_code = ctlwizchip(CW_INIT_WIZCHIP,(void*)memsize);
    if(error_code == -1)
    {
       __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "WIZCHIP Initialized fail.\r\n");
       while(1);
    }

    /* PHY link status check */
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "W5500 PHY Link Status Check\r\n");  
    do
    {
       error_code = ctlwizchip(CW_GET_PHYLINK, (void*)&tmp);
       if(error_code == -1)
        {
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Unknown PHY Link stauts.\r\n");
        }
       __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Pyhisical link is not established (status: %d)\r\n", tmp);
       nrf_delay_ms(500);
    } while(tmp == PHY_LINK_OFF);
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "W5500 PHY link status is ON\r\n");

    /* Setting retry time value and retry count */
    timeout_info.retry_cnt = 1;
    timeout_info.time_100us = 1000;	// timeout value = 10ms
    wizchip_settimeout(&timeout_info);
    
    /* Network initialization */
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Writing network information to chip...\r\n");  
    ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);

    /* DHCP initialization */
    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Initializing DHCP\r\n");  
    ethernet_dhcp_init();

    /* Socket initialization*/
    command_socket_init();
    link_monitor_socket_init();
    time_sync_socket_init();
    rx_socket_init();
}

void send_over_ethernet(uint8_t* payload_package, ethernet_package_t ethernet_package_type)
{
    uint8_t socket;
    uint16_t port;

    command_system_package_t package;
    package.identifier = 0xDEADFACE;
    get_own_MAC((uint8_t*)&package.mac);

    uint8_t length = 0; // Do not need?
    
    bool send_package = true;

    switch(ethernet_package_type)
    {
        case PKG_I_AM_ALIVE:
            socket = SOCKET_COMMAND;
            port = COMMAND_PORT;
            package.opcode = CMD_I_AM_ALIVE;

            i_am_alive_package_t i_am_alive_package;
            memcpy(&i_am_alive_package, payload_package, sizeof(i_am_alive_package_t));
            package.payload.i_am_alive_package = i_am_alive_package;

            length = sizeof(command_system_package_t);
            break;
        case PKG_COMMAND:
            socket = SOCKET_COMMAND;
            port = COMMAND_PORT;
            package.opcode = CMD_COMMAND;
            break;
        case PKG_ACK:
            socket = SOCKET_COMMAND;
            port = COMMAND_PORT;
            package.opcode = CMD_ACK;
            
            ack_package_t ack_package;
            memcpy(&ack_package, payload_package, sizeof(ack_package_t));
            package.payload.ack_package = ack_package;

            length = sizeof(command_system_package_t);
            break;
        case PKG_LINK_MONITOR: 
            /* Special because of variable size */
            socket = SOCKET_LINK_MONITOR;
            port = LINK_MONITOR_PORT;
            package.opcode = CMD_LINK_MONITOR;

            link_monitor_package_t link_monitor_package;
            memcpy(&link_monitor_package, payload_package, sizeof(link_monitor_package_t));

            length = offsetof(link_monitor_package_t, rssi_data_entry[link_monitor_package.number_of_entries]);

            int32_t err = sendto(socket, (uint8_t*)&link_monitor_package, length, (uint8_t*)TX_IP, port);

            if(err < 0)
            {
              __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Error sending packet - error code: %d\n", err);
            }

            send_package = false;
            break;
        case PKG_TIME_SYNC:
            socket = SOCKET_TIME_SYNC;
            port = TIME_SYNC_PORT;
            package.opcode = CMD_TIME_SYNC;
            break;
        default:
            __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "send_over_ethernet: Unknown package type chosen");
            send_package = false;
            break;
    }
    
    if (send_package)
    {
        int32_t err = sendto(socket, (uint8_t*)&package, length, (uint8_t*)TX_IP, port);

        if(err < 0)
        {
          __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Error sending packet - error code: %d\n", err);
        }
    }
}

void get_own_IP(uint8_t* p_IP){ 
    memcpy(p_IP, own_IP, 4);
}

void get_own_MAC(uint8_t* p_MAC){ 
    memcpy(p_MAC, own_MAC, 6);
}
