#include "user_ethernet.h"
#include "user_spi.h"
#include "socket.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "nrf_delay.h"

#ifdef MESH_ENABLED
    #include "log.h"
    #define LOG(...) __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, __VA_ARGS__);
#else
    #define LOG(...) printf(__VA_ARGS__)
#endif

///////////////////////////////////
// Default Network Configuration //
///////////////////////////////////

wiz_NetInfo gWIZNETINFO = 
{
    .ip     = {1, 1, 1, 1},
    .mac    = {0x00, 0x08, 0xdc, 0x00, 0x00, 0x00},

    .sn     = {255, 255, 255, 0},
    .gw     = {10, 0, 0, 1}, 
    .dns    = {8, 8, 8, 8},
    .dhcp   = NETINFO_DHCP 
};


void wizchip_select(void)
{
	nrf_gpio_pin_clear(SPIM0_SS_PIN);
}

void wizchip_deselect(void)
{
	nrf_gpio_pin_set(SPIM0_SS_PIN);
}


uint8_t wizchip_read()
{
	uint8_t recv_data;

	spi_master_rx(SPI0,1,&recv_data);

	return recv_data;
}

void wizchip_write(uint8_t wb)
{
	spi_master_tx(SPI0, 1, &wb);
}


static void network_init(void)
{
	ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);
    if (gWIZNETINFO.dhcp == NETINFO_DHCP) 
    {
        LOG("Obatining network information using DHCP\n");
    }
    else
    {
        //print_network_info();
    }
}


void user_ethernet_init()
{
    uint8_t tmp;
    uint8_t memsize[2][8] = {{2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}};
    wiz_NetTimeout timeout_info;
        
    gWIZNETINFO.mac[0] = (0xB0                         ) & 0xFF;
    gWIZNETINFO.mac[1] = (NRF_FICR->DEVICEADDR[0] >>  8) & 0xFF;
    gWIZNETINFO.mac[2] = (NRF_FICR->DEVICEADDR[0] >> 16) & 0xFF;
    gWIZNETINFO.mac[3] = (NRF_FICR->DEVICEADDR[0] >> 24)       ;
    gWIZNETINFO.mac[4] = (NRF_FICR->DEVICEADDR[1]      ) & 0xFF;
    gWIZNETINFO.mac[5] = (NRF_FICR->DEVICEADDR[1] >>  8) & 0xFF;

    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
    reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);

    /* WIZCHIP SOCKET Buffer initialize */
	
    LOG("W5500 memory init\r\n");

    if(ctlwizchip(CW_INIT_WIZCHIP,(void*)memsize) == -1)
    {
    	LOG("WIZCHIP Initialized fail.\r\n");
       while(1);
    }

    /* PHY link status check */
    LOG("W5500 PHY Link Status Check\r\n");
    do
    {
       if(ctlwizchip(CW_GET_PHYLINK, (void*)&tmp) == -1)
    	   LOG("Unknown PHY Link stauts.\r\n");
       LOG("Status: %d\r\n", tmp);
       nrf_delay_ms(50);
    } while(tmp == PHY_LINK_OFF);

    timeout_info.retry_cnt = 1;
    timeout_info.time_100us = 1000;	// timeout value = 10ms

    wizchip_settimeout(&timeout_info);
    
    LOG("W5500 PHY link status is ON\r\n");
    
    /* Network initialization */
    network_init();
}


void print_network_info(void)
{
    uint8_t tmpstr[6];
    
	ctlnetwork(CN_GET_NETINFO, (void*)&gWIZNETINFO);
    
	// Display Network Information
	ctlwizchip(CW_GET_ID,(void*)tmpstr);
	LOG("=== %s NET CONF ===\r\n",(char*)tmpstr);
	LOG("MAC:\t %02X:%02X:%02X:%02X:%02X:%02X\r\n",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],
		  gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
	LOG("SIP:\t %d.%d.%d.%d\r\n", gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
	LOG("GAR:\t %d.%d.%d.%d\r\n", gWIZNETINFO.gw[0],gWIZNETINFO.gw[1],gWIZNETINFO.gw[2],gWIZNETINFO.gw[3]);
	LOG("SUB:\t %d.%d.%d.%d\r\n", gWIZNETINFO.sn[0],gWIZNETINFO.sn[1],gWIZNETINFO.sn[2],gWIZNETINFO.sn[3]);
	LOG("DNS:\t %d.%d.%d.%d\r\n", gWIZNETINFO.dns[0],gWIZNETINFO.dns[1],gWIZNETINFO.dns[2],gWIZNETINFO.dns[3]);
	LOG("======================\r\n");
}
