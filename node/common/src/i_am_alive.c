#include <stdint.h>

#include "i_am_alive.h"
#include "log.h"
#include "mesh_app_utils.h"
#include "app_timer.h"
#include "hal.h"
#include "ethernet_network.h"
#include "socket.h"

APP_TIMER_DEF(IM_ALIVE_TIMER);

void send_i_am_alive_message(void)
{
    uint8_t buf[50];
    uint8_t len = 0;
    uint8_t own_MAC[6] = {0};
    get_own_MAC(own_MAC);

    #ifdef BROADCAST_ENABLED
        uint8_t target_IP[4] = {255, 255, 255, 255}; 
        uint32_t target_port = 11001;
    #else
        uint8_t target_IP[4];
        get_target_IP(target_IP);       
        uint32_t target_port = 11040;
    #endif

    if(!is_network_busy())
    {
        set_network_busy(true);
    
        sprintf((char *)&buf[0], "I AM ALIVE - %02x:%02x:%02x:%02x:%02x:%02x", own_MAC[0], own_MAC[1], own_MAC[2], own_MAC[3], own_MAC[4], own_MAC[5]);
    
        len = strlen((const char *)&buf[0]);
        int32_t err = sendto(SOCKET_UDP, &buf[0], len, target_IP, target_port);

        if(err < 0)
        {
          __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Error sending packet (send_i_am_alive_message): %d\n", err);
        }

        set_network_busy(false);
    }
}

void i_am_alive_timer_handler(void * p_unused){
//    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "I AM ALIVE TIMER\n");
    send_i_am_alive_message();
}

void i_am_alive_timer_init(void)
{
  ERROR_CHECK(app_timer_create(&IM_ALIVE_TIMER, APP_TIMER_MODE_REPEATED, i_am_alive_timer_handler));
  ERROR_CHECK(app_timer_start(IM_ALIVE_TIMER, HAL_MS_TO_RTC_TICKS(500), NULL));
}