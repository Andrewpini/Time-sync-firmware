
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "log.h"
#include "config_and_prov.h"
#include "device_state_manager.h"
#include "example_common.h"
#include "node_setup.h"

static prov_helper_uuid_filter_t m_exp_uuid;


static varde_prov_config_entry_t entries[256];
static uint8_t entry_counter = 0;

static bool busy_flag = false;

static bool search_for_entry(uint16_t addr, uint8_t* p_entry_pos)
{ 
    for(uint8_t i = 0; i < entry_counter; i++)
    {
        if(entries[i].addr == addr)
        {
            if(p_entry_pos != NULL)
            {
                *p_entry_pos = i;
            }
            return true;
        }
    }
    return false;
}

bool get_entry(uint16_t addr, varde_prov_config_entry_t* p_entry)
{
    uint8_t entry_pos;
    if(!search_for_entry(addr, &entry_pos))
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "An entry for address 0x%x does not exist", addr);
        return false;
    }
    else
    {
        memcpy(p_entry, entries + entry_pos, sizeof(varde_prov_config_entry_t));
        return  true;
    }
}

void add_entry(uint16_t addr)
{
    if(search_for_entry(addr, NULL))
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "An entry for address 0x%x already exists\n", addr);
    }
    else
    {
        entries[entry_counter].addr = addr;

        entries[entry_counter].flags.config_flag = false;

        entry_counter++;
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "An entry for address 0x%x has been added\n", addr);
    }
}

void delete_entry(uint16_t addr)
{   
    uint8_t entry_pos;
    if(!search_for_entry(addr, &entry_pos))
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "An entry for address 0x%x does not exist. No need for deletion\n", addr);
    }
    else
    {
        entry_counter--;
        memcpy(entries + entry_pos, entries + entry_counter, sizeof(varde_prov_config_entry_t));

        memset(entries + entry_counter, NULL, sizeof(varde_prov_config_entry_t));

        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "The entry for address 0x%x has been deleted\n", addr);
    }
}

bool raise_flag(uint16_t addr, varde_prov_config_flag_t flag, bool val)
{
    uint8_t entry_pos;
    if(!search_for_entry(addr, &entry_pos))
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "An entry for address 0x%x does not exist. Can't raise flag\n", addr);
    }
    else
    {
        switch(flag)
        {
            case CONFIG_FLAG:
                entries[entry_pos].flags.config_flag = val;
                break;

            case DEV_HANDLE_PRESENT_FLAG:
                entries[entry_pos].flags.dev_handle_present = val;
                break;
        }
    }
}

bool start_prov_check(uint16_t addr)
{
    uint8_t entry_pos;
    if(search_for_entry(addr, &entry_pos))
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "An entry for address 0x%x already exist\n", addr);
        return false;
    }
    else if(busy_flag)
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "The configuration module is busy. Try again in a bit\n");
    }
    else
    {
        /* Start provisioning */
        static const char * uri_list[1];
        uri_list[0] = EX_URI_LS_SERVER;
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Waiting for Server node to be provisioned ...\n");
        prov_helper_provision_next_device(PROVISIONER_RETRY_COUNT, addr,
                                           uri_list, ARRAY_SIZE(uri_list));
        prov_helper_scan_start();
        busy_flag = true;
    }
}

bool start_config_check(uint16_t addr, uint8_t* p_appkey)
{
    uint8_t entry_pos;
    if(!search_for_entry(addr, &entry_pos))
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "An entry for address 0x%x does not exist\n", addr);
    }    
    else if(entries[entry_pos].flags.config_flag)
    {
       __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Device with address 0x%x is already configured\n", addr);
    }
    else if(busy_flag)
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "The configuration module is busy. Try again in a bit\n");
    }
    else
    {
       __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Configuring device with address 0x%x\n", addr);
       node_setup_start(addr, PROVISIONER_RETRY_COUNT, p_appkey, APPKEY_INDEX);
       busy_flag = true;
    }  
}

bool start_reset_check(uint16_t addr)
{
    uint8_t entry_pos;
    if(!search_for_entry(addr, &entry_pos))
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "An entry for address 0x%x does not exist\n", addr);
    }    
    else if(busy_flag)
    {
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "The configuration module is busy. Try again in a bit\n");
    }
    else
    {
       __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Removing device with address 0x%x\n", addr);
       reset_node(addr);
       busy_flag = true;
    }  
}

void clear_busy_flag(void)
{
    busy_flag = false;
}

void restore_entries(void)
{
    dsm_handle_t handles[256];
    uint32_t handle_count = sizeof(handles)/sizeof(handles[0]);
    dsm_address_get_all(handles, &handle_count);
    
    nrf_mesh_address_t address;
    for(int i = 0; i < handle_count; i++)
    {
        uint32_t return_code = dsm_address_get(handles[i], &address);
        if(return_code == NRF_SUCCESS && address.type == NRF_MESH_ADDRESS_TYPE_UNICAST)
        {
            add_entry(address.value);
        }
    }

}