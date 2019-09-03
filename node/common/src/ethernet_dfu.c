#include <stdbool.h>

#include "ethernet_dfu.h"
#include "mesh_app_utils.h"
#include "mesh_flash.h"
#include "boards.h"
#include "ethernet.h"
#include "command_system.h"

static uint8_t own_ip[4];
static uint8_t server_ip[4];

static uint16_t write_token;
static bool write_flag = false;
static uint16_t erase_token;
static bool erase_flag = false;

static void dfu_erase_flash_page(void)
{
    /* *** The rest of functionality is placed in mesh_flash_user_cb() *** */
    flash_operation_t flash_parameters_erase;

    flash_parameters_erase.type = FLASH_OP_TYPE_ERASE;

    flash_parameters_erase.params.erase.p_start_addr = (uint32_t *)0xFE000;
    flash_parameters_erase.params.erase.length = 4096;

    const flash_operation_t * p_flash_parameters_erase = &flash_parameters_erase;
  
    ERROR_CHECK(mesh_flash_op_push(MESH_FLASH_USER_APP, p_flash_parameters_erase, &erase_token));
}                                                             

static void dfu_write_ip_addresses_to_flash(uint32_t * flash_address)
{
    /* *** The rest of functionality is placed in mesh_flash_user_cb() *** */
    uint32_t ip_addresses_array[2];

    ip_addresses_array[0] = *((uint32_t*)own_ip);
    ip_addresses_array[1] = *((uint32_t*)server_ip);

    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Writing to flash address %x\n", (uint32_t)flash_address);
  
    flash_operation_t flash_parameters;

    flash_parameters.type = FLASH_OP_TYPE_WRITE;

    flash_parameters.params.write.p_start_addr = flash_address;
    flash_parameters.params.write.length = 8;
    flash_parameters.params.write.p_data = ip_addresses_array;

    const flash_operation_t * p_flash_parameters = &flash_parameters;

    ERROR_CHECK(mesh_flash_op_push(MESH_FLASH_USER_APP, p_flash_parameters, &write_token));
}

void dfu_start_process(void)
{
    /* Finding the next empty flash address and start flash opereration */
    uint32_t* flash_address = (uint32_t*)0xFE000;

    for(uint16_t i=0; i<4096; i++)
    {
      if (flash_address >= (uint32_t*)0xFF000)
      {
        erase_flag = true;
        dfu_erase_flash_page();
        break;
      } 
      else if (*flash_address == 0xFFFFFFFF)
      {
        write_flag = true;
        dfu_write_ip_addresses_to_flash(flash_address);
        break;
      } 
      else
      {
        flash_address += 1;
      }
    }
}

static void dfu_set_flag_and_reset(void)
{ 
    ERROR_CHECK(sd_power_gpregret_set(0, 0xAA));

    uint32_t gpreget_info = 0x00;
              
    sd_power_gpregret_get(0, &gpreget_info);
  
    while(gpreget_info != 0xAA)
    {
      sd_power_gpregret_get(0, &gpreget_info);
    }

    ERROR_CHECK(sd_nvic_SystemReset());
}

static void mesh_flash_user_cb(mesh_flash_user_t user, const flash_operation_t * p_op, uint16_t token)
{
    if (token == write_token)
    {
      if (write_flag)
      {
        write_flag = false;
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Successfully written IP addresses to flash\n");
        dfu_set_flag_and_reset();
      }
    }
    else if (token == erase_token)
    {
      if (erase_flag)
      {
        erase_flag = false;
        write_flag = true;
        __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Successfully erased flash page\n");
        uint32_t* flash_address = (uint32_t*)0xFE000;
        dfu_write_ip_addresses_to_flash(flash_address);
      }
    }
}

void dfu_init()
{
    ERROR_CHECK(sd_power_gpregret_clr(0, 0xFF));
    mesh_flash_user_callback_set(MESH_FLASH_USER_APP, mesh_flash_user_cb);
}

void dfu_start()
{
    get_own_ip(own_ip);
    get_server_ip(server_ip);

    if (!(server_ip[0] == 1))
    { 
      dfu_start_process();
    } else
    {
      __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "No server IP - can not start DFU\n");
    }
}