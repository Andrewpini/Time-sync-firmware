
#ifndef CMD_SYSTEM_H
#define CMD_SYSTEM_H

bool is_scanning_enabled(void);

bool is_advertising_enabled(void);

bool is_who_am_i_enabled(void);

bool is_server_ip_received(void);

void get_server_ip(uint8_t * buf, uint8_t len);

void dhcp_init(void);

void check_ctrl_cmd(void);

bool is_network_busy(void);

void set_network_busy(bool val);

void get_target_ip_and_port(uint8_t* p_IP, uint32_t* p_port);

#endif
