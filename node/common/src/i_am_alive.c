#include <stdint.h>

#include "i_am_alive.h"
#include "config.h"
#include "log.h"
#include "mesh_app_utils.h"
#include "app_timer.h"
#include "hal.h"
#include "ethernet.h"
#include "command_system.h"
#include "socket.h"
#include "device_state_manager.h"

APP_TIMER_DEF(IM_ALIVE_TIMER);

void send_i_am_alive_message(void)
{
    dsm_local_unicast_address_t node_element_address;
    dsm_local_unicast_addresses_get(&node_element_address);
    
    i_am_alive_package_t i_am_alive_package;

    get_own_ip((uint8_t*)&i_am_alive_package.ip);
    i_am_alive_package.element_address = node_element_address.address_start;

    send_over_ethernet((uint8_t*)&i_am_alive_package, CMD_I_AM_ALIVE);
}

void i_am_alive_timer_handler(void * p_unused){
    send_i_am_alive_message();
}

void i_am_alive_timer_init(void)
{
  ERROR_CHECK(app_timer_create(&IM_ALIVE_TIMER, APP_TIMER_MODE_REPEATED, i_am_alive_timer_handler));
}

void i_am_alive_timer_start(void)
{
  ERROR_CHECK(app_timer_start(IM_ALIVE_TIMER, HAL_MS_TO_RTC_TICKS(I_AM_ALIVE_SENDING_INTERVAL), NULL));
}