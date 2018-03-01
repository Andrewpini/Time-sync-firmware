

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "radio.h"
#include "advertise.h"

#define BD_ADDR_OFFS                (3)     /* BLE device address offest of the beacon advertising pdu. */
#define M_BD_ADDR_SIZE              (6)     /* BLE device address size. */

static uint8_t m_adv_pdu[40];               /* The RAM representation of the advertising PDU. */


void advertise_init(void)
{
    m_adv_pdu[0] = 0x42;
    m_adv_pdu[1] = 0;
    m_adv_pdu[2] = 0;

    if ( ( NRF_FICR->DEVICEADDR[0]           != 0xFFFFFFFF)
            ||   ((NRF_FICR->DEVICEADDR[1] & 0xFFFF) != 0xFFFF) )
    {
        m_adv_pdu[BD_ADDR_OFFS    ] = (NRF_FICR->DEVICEADDR[0]      ) & 0xFF;
        m_adv_pdu[BD_ADDR_OFFS + 1] = (NRF_FICR->DEVICEADDR[0] >>  8) & 0xFF;
        m_adv_pdu[BD_ADDR_OFFS + 2] = (NRF_FICR->DEVICEADDR[0] >> 16) & 0xFF;
        m_adv_pdu[BD_ADDR_OFFS + 3] = (NRF_FICR->DEVICEADDR[0] >> 24)       ;
        m_adv_pdu[BD_ADDR_OFFS + 4] = (NRF_FICR->DEVICEADDR[1]      ) & 0xFF;
        m_adv_pdu[BD_ADDR_OFFS + 5] = (NRF_FICR->DEVICEADDR[1] >>  8) & 0xFF;
    }
    else
    {
        static const uint8_t random_bd_addr[M_BD_ADDR_SIZE] = {0xE2, 0xA3, 0x01, 0xE7, 0x61, 0xF8};
        memcpy(&(m_adv_pdu[3]), &(random_bd_addr[0]), M_BD_ADDR_SIZE);
    }
    m_adv_pdu[1] = (m_adv_pdu[1] == 0) ? M_BD_ADDR_SIZE : m_adv_pdu[1];

    static const uint8_t adv_data[31] = 
                    {
                        0x02,                   
                        0x01, 0x04,                                 // Flags: Not BR/EDR: true
                        0x03,                   
                        0x19, 0x00, 0x03,                           // Appearance: Health thermometer
                        0x03,                   
                        0x09, 0x6A, 0x62,                           // Full device name: 'jb'
                        0x07,                   
                        0x03, 0x02, 0x18, 0x09, 0x18, 0x0A, 0x18,   // Services: Immediate alert (0x1802), health thermometer (0x1809) and device information (0x180A)
                        0x07,                   
                        0x16, 0x09, 0x18, 0xA3, 0x70, 0x45, 0x41,   // Service data for 0x1809, health thermometer
                    };
    memcpy(&(m_adv_pdu[3 + M_BD_ADDR_SIZE]), &(adv_data[0]), sizeof(adv_data));
    m_adv_pdu[1] = M_BD_ADDR_SIZE + sizeof(adv_data);
    radio_buffer_configure (&m_adv_pdu[0]);
}


void advertise_ble_channel_once(uint8_t channel) 
{
    radio_init(channel);
    radio_tx_prepare();
    while (NRF_RADIO->EVENTS_DISABLED == 0) {}
    radio_disable();

}
