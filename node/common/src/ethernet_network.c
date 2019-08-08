#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "ethernet_network.h"
#include "w5500.h"
#include "socket.h"
#include "pwm.h"
#include "boards.h"
#include "dhcp.h"
#include "dhcp_cb.h"
#include "time_sync_timer.h"
#include "user_ethernet.h"

#ifdef MESH_ENABLED
    #include "log.h"
    #define LOG(...) __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, __VA_ARGS__);
#else
    #define LOG(...) printf(__VA_ARGS__)
#endif

#define DHCP_ENABLED                        1
#define SOCKET_CONFIG                       0
#define SOCKET_TCPC                         1
#define SOCKET_DHCP                         2
#define TX_BUF_SIZE                         2048

static uint8_t TX_BUF[TX_BUF_SIZE];


static uint8_t target_IP[4]                 = {10, 0, 0, 4};      // Arbitrary fallback
static uint32_t target_port                 = 15000;
static uint8_t own_MAC[6]          = {0};
static uint8_t own_IP[4]           = {0};

/* State variables */
static volatile bool m_connected              = false;
static volatile bool m_server_ip_received     = false;

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
void ethernet_broadcast_send(uint8_t * buf, uint8_t len) 
{
    uint8_t broadcast_ip[] = {255, 255, 255, 255};
    uint16_t broadcast_port = 11001;
    
    sendto(SOCKET_UDP, &buf[0], len, broadcast_ip, broadcast_port);
}

/// ***  SECTION: Event handlers   *** ///

void on_connect(void)
{
    m_connected = true;
}

void on_disconnect(void)
{
    m_connected = true;
    pwm_set_duty_cycle(LED_HP, 0);
}

// Function to get and store server IP and port number to which all data will be sent
void get_server_ip(uint8_t * buf, uint8_t len)
{
    static uint8_t str[SERVER_IP_PREFIX_LEN] = {0};
    const uint8_t pos[] = SERVER_IP_PREFIX;
    strncpy((void *)str, (const void *)buf, SERVER_IP_PREFIX_LEN);
    int8_t compare = strncmp((void *)str, (const void *)pos, SERVER_IP_PREFIX_LEN);

    if (compare == 0)
    {
        // Server IP address prefix is found
        m_server_ip_received = true;
        uint8_t ip_str[len - SERVER_IP_PREFIX_LEN];
        uint8_t ip[4] = {0};
        uint16_t port = 0;
        uint8_t index = 0;
        uint8_t *p_ip_str = &(ip_str[0]);

        memcpy((char *)ip_str, (const char *)&(buf[SERVER_IP_PREFIX_LEN]), (len - SERVER_IP_PREFIX_LEN));

        while (index < 5) 
        {
            if (isdigit((unsigned char)*p_ip_str)) {
                if (index < 4)
                {
                    ip[index] *= 10;
                    ip[index] += *p_ip_str - '0';
                }
                else
                {
                    port *= 10;
                    port += *p_ip_str - '0';
                }
            } else {
                index++;
            }
            p_ip_str += 1;
        }
        memcpy((char *)target_IP, (const char *)ip, 4);
        target_port = port;
        LOG("Server IP: %d.%d.%d.%d : %d\r\n", target_IP[0], target_IP[1], target_IP[2], target_IP[3], target_port);

    }
}


void dhcp_init(void)
{
    if(DHCP_ENABLED)
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
                LOG("This device' IP: %d.%d.%d.%d\r\n", own_IP[0], own_IP[1], own_IP[2], own_IP[3]);
                print_network_info();
                break;
            }
            else if(ret == DHCP_FAILED)
            {
                 dhcp_retry++;  
            }

            if(dhcp_retry > 10)
            {
                break;
            }
        }
    }
    else 							
    {
        // Fallback config if DHCP fails
    }
}

bool is_connected(void){
    return m_connected;
}

bool is_server_IP_received(void){
    return  m_server_ip_received;
}

void set_server_IP_received(bool val){
    m_server_ip_received = val;
}

void set_target_IP(uint8_t* p_IP){
    memcpy(target_IP, p_IP, 4);
}

void get_target_IP_and_port(uint8_t* p_IP, uint32_t* p_port){ 
    memcpy(p_IP, target_IP, 4);
    *p_port = target_port;
}

void get_target_IP(uint8_t* p_IP){
    memcpy(p_IP, target_IP, 4);
}

void get_own_IP(uint8_t* p_IP){ 
    memcpy(p_IP, own_IP, 4);
}

void get_own_MAC(uint8_t* p_MAC){ 
    memcpy(p_MAC, own_MAC, 6);
}