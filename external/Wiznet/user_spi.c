 /* Copyright (c) 2009 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is confidential property of Nordic
 * Semiconductor ASA.Terms and conditions of usage are described in detail
 * in NORDIC SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */
#include "user_spi.h"
#include <string.h>
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"
#include "nrf_log.h"

#define SPI_TIMER_TIMEOUT_INTERVAL_MS           1000

static volatile bool timeout_triggered = false;
static volatile bool triggered_count = 0;

static SPIConfig_t spi_config_table[2];
static NRF_SPI_Type *spi_base[2] = {NRF_SPI0, NRF_SPI1};
// static NRF_SPI_Type *SPI;


#define SPI_INSTANCE    0 /**< SPI instance index. */
static const            nrf_drv_spi_t spi_inst = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);    /**< SPI instance. */

void spi_master_timer_init(void)
{
    // Timer for sampling of RSSI values
    NRF_TIMER4->MODE                = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;                                 // Timer mode
    NRF_TIMER4->BITMODE             = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;                     // 32-bit timer
    NRF_TIMER4->PRESCALER           = 4 << TIMER_PRESCALER_PRESCALER_Pos;                                           // Prescaling: 16 MHz / 2^PRESCALER = 16 MHz / 16 = 1 MHz timer
    NRF_TIMER4->CC[0]               = SPI_TIMER_TIMEOUT_INTERVAL_MS * 1000;                                        // Compare event
    NRF_TIMER4->SHORTS              = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;       // Clear compare event on event
    NRF_TIMER4->INTENSET            = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;
    NVIC_ClearPendingIRQ(TIMER4_IRQn);
    NVIC_EnableIRQ(TIMER4_IRQn);
}
void TIMER4_IRQHandler(void) 
{
    NRF_TIMER4->EVENTS_COMPARE[0]   = 0;
    //NRF_TIMER4->INTENSET            = TIMER_INTENSET_COMPARE0_Disabled << TIMER_INTENSET_COMPARE0_Pos;
    timeout_triggered = true;
    triggered_count += 1;
//    if (triggered_count > 10) *** ALWAYS RETURNS FALSE
//    {
//        triggered_count = 0;
//    }
}

void spi_master_timeout_start(void)
{
    //NRF_TIMER4->INTENSET            = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;
    NRF_TIMER4->TASKS_START         = 1;
    timeout_triggered = false;
}

void spi_master_timeout_stop(void)
{
    //NRF_TIMER4->INTENSET            = TIMER_INTENSET_COMPARE0_Disabled << TIMER_INTENSET_COMPARE0_Pos;
    NRF_TIMER4->TASKS_STOP          = 1;
    timeout_triggered = false;
}


void spi0_master_init()
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
    /*
    SPIConfig_t spi_info = {.Config.Fields.BitOrder = SPI_BITORDER_MSB_LSB,
                        .Config.Fields.Mode     = SPI_MODE3,
                        .Frequency              = SPI_FREQ_1MBPS,
                        .Pin_SCK                = SPIM0_SCK_PIN,
                        .Pin_MOSI               = SPIM0_MOSI_PIN,
                        .Pin_MISO               = SPIM0_MISO_PIN,
                        .Pin_CSN                = SPIM0_SS_PIN};	
    spi_master_init(SPI0, &spi_info);
    spi_master_timer_init();*/
}

uint32_t* spi_master_init(SPIModuleNumber spi_num, SPIConfig_t *spi_config)
{
    if(spi_num > 1)
    {
        return 0;
    }
    memcpy(&spi_config_table[spi_num], spi_config, sizeof(SPIConfig_t));

    /* Configure GPIO pins used for pselsck, pselmosi, pselmiso and pselss for SPI0 */
    nrf_gpio_cfg_output(spi_config->Pin_SCK);
    nrf_gpio_cfg_output(spi_config->Pin_MOSI);
    nrf_gpio_cfg_input(spi_config->Pin_MISO, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_output(spi_config->Pin_CSN);
    nrf_gpio_pin_clear(spi_config->Pin_CSN);

    /* Configure pins, frequency and mode */
    spi_base[spi_num]->PSELSCK  = spi_config->Pin_SCK;
    spi_base[spi_num]->PSELMOSI = spi_config->Pin_MOSI;
    spi_base[spi_num]->PSELMISO = spi_config->Pin_MISO;
   // nrf_gpio_pin_clear(spi_config->Pin_CSN); /* disable Set slave select (inactive high) */

    spi_base[spi_num]->FREQUENCY = (uint32_t)spi_config->Frequency << 24;

    spi_base[spi_num]->CONFIG = spi_config->Config.SPI_Cfg;

    spi_base[spi_num]->EVENTS_READY = 0;
    /* Enable */
    spi_base[spi_num]->ENABLE = (SPI_ENABLE_ENABLE_Enabled << SPI_ENABLE_ENABLE_Pos);

    return (uint32_t *)spi_base[spi_num];
}

bool spi_master_tx(SPIModuleNumber spi_num, uint16_t transfer_size, const uint8_t *tx_data)
{
    // volatile uint32_t dummyread;

    if(tx_data == 0)
    {
        return false;
    }
    
    if(nrf_drv_spi_transfer(&spi_inst, tx_data, 1, NULL, 0) != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("SPI failed\r\n"); 
        return false;
    }
    
    return true;

//    SPI = spi_base[spi_num];
//    /* enable slave (slave select active low) */
//    //nrf_gpio_pin_clear(spi_config_table[spi_num].Pin_CSN);

//    SPI->EVENTS_READY = 0;

//    SPI->TXD = (uint32_t)*tx_data++;

//    while(--transfer_size)
//    {
//        SPI->TXD =  (uint32_t)*tx_data++;

//        /* Wait for the transaction complete or timeout (about 10ms - 20 ms) */
//        while (SPI->EVENTS_READY == 0);

//        /* clear the event to be ready to receive next messages */
//        SPI->EVENTS_READY = 0;

//        dummyread = SPI->RXD;
//    }

//    /* Wait for the transaction complete or timeout (about 10ms - 20 ms) */
//    spi_master_timeout_start();
//    while ((SPI->EVENTS_READY == 0) && !timeout_triggered) {};
//    if (timeout_triggered)
//        return false;
//    spi_master_timeout_stop();

//    dummyread = SPI->RXD;

//    /* disable slave (slave select active low) */
//    //nrf_gpio_pin_set(spi_config_table[spi_num].Pin_CSN);

//    return true;

}

bool spi_master_rx(SPIModuleNumber spi_num, uint16_t transfer_size, uint8_t *rx_data)
{    
     if(rx_data == 0)
    {
        return false;
    }
    
    if(nrf_drv_spi_transfer(&spi_inst, NULL, 0, rx_data, 1) != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("SPI failed\r\n"); 
        return false;
    }
    
    return true;

//    SPI = spi_base[spi_num];

//    /* enable slave (slave select active low) */
//    //nrf_gpio_pin_clear(spi_config_table[spi_num].Pin_CSN);

//    SPI->EVENTS_READY = 0;

//    SPI->TXD = 0;

//    while(--transfer_size)
//    {
//        SPI->TXD = 0;

//        /* Wait for the transaction complete or timeout (about 10ms - 20 ms) */
//        while (SPI->EVENTS_READY == 0);

//        /* clear the event to be ready to receive next messages */
//        SPI->EVENTS_READY = 0;

//        *rx_data++ = SPI->RXD;
//    }

//    /* Wait for the transaction complete or timeout (about 10ms - 20 ms) */
//    spi_master_timeout_start();
//    while ((SPI->EVENTS_READY == 0) && !timeout_triggered) {};
//    if (timeout_triggered)
//        return false;
//    spi_master_timeout_stop();
//    
//    *rx_data = SPI->RXD;

//    /* disable slave (slave select active low) */
//   // nrf_gpio_pin_set(spi_config_table[spi_num].Pin_CSN);

//    return true;

}
