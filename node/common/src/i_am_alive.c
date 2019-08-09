#include <stdint.h>

#include "i_am_alive.h"
#include "log.h"
#include "mesh_app_utils.h"
#include "app_timer.h"
#include "hal.h"
#include "ethernet.h"
#include "socket.h"

APP_TIMER_DEF(IM_ALIVE_TIMER);

void send_i_am_alive_message(void)
{
    uint8_t buf[50];
    uint8_t len = 0;
    uint8_t own_MAC[6] = {0};
    get_own_MAC(own_MAC);
    
    sprintf((char *)&buf[0], "I AM ALIVE - %02x:%02x:%02x:%02x:%02x:%02x", own_MAC[0], own_MAC[1], own_MAC[2], own_MAC[3], own_MAC[4], own_MAC[5]);

    len = strlen((const char *)&buf[0]);

    send_over_ethernet(&buf[0] , len);
}

void i_am_alive_timer_handler(void * p_unused){
//    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "I AM ALIVE TIMER\n");
    send_i_am_alive_message();
}

void i_am_alive_timer_init(void)
{
  ERROR_CHECK(app_timer_create(&IM_ALIVE_TIMER, APP_TIMER_MODE_REPEATED, i_am_alive_timer_handler));
}

void i_am_alive_timer_start(void)
{
    ERROR_CHECK(app_timer_start(IM_ALIVE_TIMER, HAL_MS_TO_RTC_TICKS(500), NULL));
}