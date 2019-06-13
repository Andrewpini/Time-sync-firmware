#ifndef VARDE_PROV_CONFIG_H__
#define VARDE_PROV_CONFIG_H__

#include "provisioner_helper.h"

typedef struct
{
    uint16_t addr;
    struct
    {
        bool config_flag;
        bool dev_handle_present;
    } flags;
} varde_prov_config_entry_t;

typedef  enum
{
    CONFIG_FLAG,
    DEV_HANDLE_PRESENT_FLAG
} varde_prov_config_flag_t;

void add_entry(uint16_t addr);

void delete_entry(uint16_t addr); 

bool get_entry(uint16_t addr, varde_prov_config_entry_t* p_entry);

bool raise_flag(uint16_t addr, varde_prov_config_flag_t flag, bool val);

bool start_prov_check(uint16_t addr);

bool start_config_check(uint16_t addr, uint8_t* p_appkey);

void restore_entries(void);

void clear_busy_flag(void);

bool start_reset_check(uint16_t addr);
#endif