
#ifndef APP_NETWORK_H
#define APP_NETWORK_H

//#define LOG(...)                            printf(__VA_ARGS__)       
#define SOCKET_UDP                          3
#define SOCKET_BROADCAST                    6
#define UDP_PORT                            17545
#define UDP_FLAGS                           0x00
#define BROADCAST_PORT                      10000
#define MULTICAST_PORT                      3000


void dhcp_init(void);

void broadcast_init(void);

void connection_init(void);

bool is_connected(void);
 
void get_server_ip(uint8_t * buf, uint8_t len);

bool is_network_busy(void);

void set_network_busy(bool val);

bool is_server_IP_received(void);

void set_server_IP_received(bool val);

void get_target_IP_and_port(uint8_t* p_IP, uint32_t* p_port);

void get_own_IP(uint8_t* p_IP);

void get_own_MAC(uint8_t* p_MAC);
#endif
