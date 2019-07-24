
#ifndef APP_NETWORK_H
#define APP_NETWORK_H

#define SOCKET_UDP                          3
#define SOCKET_BROADCAST                    6
#define UDP_PORT                            17545
#define UDP_FLAGS                           0x00
#define BROADCAST_PORT                      10000
#define MULTICAST_PORT                      3000


void ethernet_broadcast_send(uint8_t * buf, uint8_t len);

void dhcp_init(void);

void broadcast_init(void);

void connection_init(void);

bool is_connected(void);
 
void get_server_ip(uint8_t * buf, uint8_t len);

bool is_network_busy(void);

void set_network_busy(bool val);

bool is_server_IP_received(void);

void set_server_IP_received(bool val);

void set_target_IP(uint8_t* p_IP); // TODO: Is "server" and "target" the same?

void get_target_IP_and_port(uint8_t* p_IP, uint32_t* p_port);

void get_target_IP(uint8_t* p_IP);

void get_own_IP(uint8_t* p_IP);

void get_own_MAC(uint8_t* p_MAC);
#endif
