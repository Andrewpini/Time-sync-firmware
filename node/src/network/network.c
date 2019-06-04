#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "network.h"
#include "w5500.h"
#include "socket.h"
#include "pwm.h"
#include "gpio.h"

/* State variables */
static volatile bool m_connected              = false;

/* Forward declarations */
void connection_init(void);
void on_connect(void);

/// ***  SECTION: Network functions   *** ///

// Function to establish UDP socket 
void connection_init(void) 
{
        socket(SOCKET_UDP, Sn_MR_UDP, UDP_PORT, UDP_FLAGS);
        on_connect();
}

// Initiates socket for UDP broadcast messages
void broadcast_init(void)
{
    uint8_t flag = SF_IO_NONBLOCK;
    socket(SOCKET_BROADCAST, Sn_MR_UDP, BROADCAST_PORT, flag);
}

// Function to broadcast data to all nodes on the network
void broadcast_send(uint8_t * buf, uint8_t len) 
{
    uint8_t broadcast_ip[] = {255, 255, 255, 255};
    uint16_t broadcast_port = 10000;
    
    sendto(SOCKET_UDP, &buf[0], len, broadcast_ip, broadcast_port);
}

/// ***  SECTION: Event handlers   *** ///

void on_connect(void)
{
    m_connected = true;
    pwm_set_duty_cycle(LED_HP, LED_HP_DEFAULT_DUTY_CYCLE);
}

void on_disconnect(void)
{
    m_connected = true;
    pwm_set_duty_cycle(LED_HP, 0);
}

bool is_connected(void){
    return m_connected;
}