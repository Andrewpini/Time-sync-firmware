
#ifndef APP_NETWORK_H
#define APP_NETWORK_H

#define LOG(...)                            printf(__VA_ARGS__)       
#define SOCKET_UDP                          3
#define SOCKET_BROADCAST                    6
#define UDP_PORT                            17545
#define UDP_FLAGS                           0x00
#define BROADCAST_PORT                      10000


void broadcast_init(void);

void connection_init(void);

bool is_connected(void);
#endif
