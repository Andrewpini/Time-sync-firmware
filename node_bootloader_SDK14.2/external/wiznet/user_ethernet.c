#include "user_ethernet.h"
#include "socket.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_drv_spi.h"
#include "boards.h"
#include "ethernet_dfu.h"



#define NRF_DRV_SPI
//#define NRF_LOG_MODULE_NAME "ETH"


#define SPI_INSTANCE    0 /**< SPI instance index. */
static const            nrf_drv_spi_t spi_inst = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);    /**< SPI instance. */

///////////////////////////////////
// Default Network Configuration //
///////////////////////////////////
wiz_NetInfo gWIZNETINFO = { .mac    = {0x00, 0x08, 0xdc,0x00, 0xab, 0xfe},
                            .ip     = {1, 1, 1, 1}, 
                            .sn     = {255,255,255,0},
                            .gw     = {10, 0, 0, 1}, 
                            .dns    = {8,8,8,8},
                            .dhcp   = NETINFO_STATIC };

void wizchip_select(void)
{
	nrf_gpio_pin_clear(SPIM0_SS_PIN);
}

void wizchip_deselect(void)
{
	nrf_gpio_pin_set(SPIM0_SS_PIN);
}

void wizchip_read_burst(uint8_t *pBuf, uint16_t len)
{
    if(nrf_drv_spi_transfer(&spi_inst, NULL, 0, pBuf, len) != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("SPI failed"); 
    }
}

void wizchip_write_burst(uint8_t *pBuf, uint16_t len)
{
    if(nrf_drv_spi_transfer(&spi_inst, pBuf, len, NULL, 0) != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("SPI failed"); 
    }
}

uint8_t wizchip_read()
{
    uint8_t rx_buf=0;
#ifdef NRF_DRV_SPI
    if(nrf_drv_spi_transfer(&spi_inst, NULL, 0, &rx_buf, 1) != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("SPI failed"); 
    }
#else
	spi_master_rx(SPI0, 1, &rx_buf);
#endif /* NRF_DRV_SPI */
	return rx_buf;
}

void wizchip_write(uint8_t wb)
{
#ifdef NRF_DRV_SPI
    if(nrf_drv_spi_transfer(&spi_inst, &wb, 1, NULL, 0) != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("SPI failed"); 
    }
#else
	spi_master_tx(SPI0, 1, &wb);
#endif /* NRF_DRV_SPI */
}

static void spi_drv_init(void)
{
#ifdef NRF_DRV_SPI
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.sck_pin   = SPIM0_SCK_PIN;
    spi_config.mosi_pin  = SPIM0_MOSI_PIN;
    spi_config.miso_pin  = SPIM0_MISO_PIN;
    spi_config.ss_pin    = NRF_DRV_SPI_PIN_NOT_USED;
    spi_config.orc       = 0x0;
    spi_config.mode      = NRF_DRV_SPI_MODE_3;
    spi_config.frequency = NRF_DRV_SPI_FREQ_4M;
    nrf_gpio_cfg_output(SPIM0_SS_PIN); /* For manual controlling CS Pin */

    nrf_drv_spi_init(&spi_inst, &spi_config, NULL, NULL);
#else
    spi0_master_init();
#endif /* NRF_DRV_SPI */
}

void user_ethernet_init(void)
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
			
		uint32_t* p_own_ip = (uint32_t*)0xFE000;
	
		for(uint16_t i=0; i<4096; i++)
		{
			if (p_own_ip >= (uint32_t*)0xFF000)
			{
				while(true)
				{
				}
			} 
			else if (*p_own_ip == 0xFFFFFFFF)
			{
				p_own_ip -= 2;
				break;
			} 
			else
			{
				p_own_ip += 1;
			}
		}
		
		uint32_t own_ip = *p_own_ip;
			
		gWIZNETINFO.ip[0] = ((uint8_t*)&own_ip)[0];
		gWIZNETINFO.ip[1] = ((uint8_t*)&own_ip)[1];
		gWIZNETINFO.ip[2] = ((uint8_t*)&own_ip)[2];
		gWIZNETINFO.ip[3] = ((uint8_t*)&own_ip)[3];
		
//		gWIZNETINFO.ip[0] = 10;
//		gWIZNETINFO.ip[1] = 0;
//		gWIZNETINFO.ip[2] = 0;
//		gWIZNETINFO.ip[3] = 60;

    spi_drv_init();

#ifdef NRF_DRV_SPI
    //reg_wizchip_spiburst_cbfunc(wizchip_read_burst,wizchip_write_burst);
#endif /* NRF_DRV_SPI */
    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
    //reg_wizchip_cs_cbfunc(NULL, NULL);
    reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);

    /* WIZCHIP SOCKET Buffer initialize */

    NRF_LOG_RAW_INFO("W5500 memory init");

    if(ctlwizchip(CW_INIT_WIZCHIP,(void*)memsize) == -1)
    {
        NRF_LOG_RAW_INFO("WIZCHIP Initialized fail");
        while(1);
    }

    /* PHY link status check */
    NRF_LOG_RAW_INFO("W5500 PHY Link Status Check");
    do
    {
        if(ctlwizchip(CW_GET_PHYLINK, (void*)&tmp) == -1)
            NRF_LOG_RAW_INFO("Unknown PHY Link stauts");
    }while(tmp == PHY_LINK_OFF);

    timeout_info.retry_cnt = 1;
    timeout_info.time_100us = 0x3E8;	// timeout value = 10ms

    wizchip_settimeout(&timeout_info);
    /* Network initialization */
    network_init();
}

void network_init(void)
{
    uint8_t tmpstr[6];
    ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);
    memset((void*)&gWIZNETINFO,0x00,sizeof(gWIZNETINFO));
    ctlnetwork(CN_GET_NETINFO, (void*)&gWIZNETINFO);

    // Display Network Information
    ctlwizchip(CW_GET_ID,(void*)tmpstr);
    NRF_LOG_RAW_INFO("\r\n=== %s NET CONF ===\r\n", (uint32_t)tmpstr);
    NRF_LOG_RAW_INFO("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],
            gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
    NRF_LOG_RAW_INFO("SIP: %d.%d.%d.%d\r\n", gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
    NRF_LOG_RAW_INFO("GAR: %d.%d.%d.%d\r\n", gWIZNETINFO.gw[0],gWIZNETINFO.gw[1],gWIZNETINFO.gw[2],gWIZNETINFO.gw[3]);
    NRF_LOG_RAW_INFO("SUB: %d.%d.%d.%d\r\n", gWIZNETINFO.sn[0],gWIZNETINFO.sn[1],gWIZNETINFO.sn[2],gWIZNETINFO.sn[3]);
    NRF_LOG_RAW_INFO("DNS: %d.%d.%d.%d\r\n", gWIZNETINFO.dns[0],gWIZNETINFO.dns[1],gWIZNETINFO.dns[2],gWIZNETINFO.dns[3]);
    NRF_LOG_RAW_INFO("======================\r\n");
}
