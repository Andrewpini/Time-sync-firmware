
#ifndef CMD_SYSTEM_H
#define CMD_SYSTEM_H

bool is_scanning_enabled(void);

bool is_advertising_enabled(void);

bool is_who_am_i_enabled(void);

bool is_server_ip_received(void);

void get_server_ip(uint8_t * buf, uint8_t len);

void dhcp_init(void);

void check_ctrl_cmd(void);

void send_timing_samples(int drift, uint32_t time_tic);
#endif
