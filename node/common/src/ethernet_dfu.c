#include <stdbool.h>

#include "ethernet_dfu.h"
#include "mesh_app_utils.h"
#include "mesh_flash.h"

//  __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "HEI\r\n");

void dfu_set_button_flag()
{
  NVIC_SetPriority(GPIOTE_IRQn, 6);
  dfu_button_flag = true;
}

void dfu_clear_button_flag()
{
  dfu_button_flag = false;
  NVIC_SetPriority(GPIOTE_IRQn, 1);
}

uint32_t dfu_clear_bootloader_flag()
{
  uint32_t error_code_dfu = sd_power_gpregret_clr(0, 0xFF);

  return error_code_dfu;
}

void dfu_initiate_and_reset()
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

void dfu_button_initiate_and_reset() // Not used now - will go into bootloader and into an spin lock until BUTTON 0 is pressed or the bootloader timeout timer kicks inn
{ 
    ERROR_CHECK(sd_power_gpregret_set(0, 0xBB));

    uint32_t gpreget_info = 0x00;
		
    sd_power_gpregret_get(0, &gpreget_info);
    
    while(gpreget_info != 0xBB)
    {
      sd_power_gpregret_get(0, &gpreget_info);
    }

    ERROR_CHECK(sd_nvic_SystemReset());
}

void dfu_write_own_ip(uint8_t * p_own_ip_uint8)
{
  uint32_t * p_dfu_own_ip = (uint32_t *)0xFE004;

  uint8_t stored_own_ip[4];

  stored_own_ip[0] = (uint8_t)*p_dfu_own_ip;
  stored_own_ip[1] = (uint8_t)*(p_dfu_own_ip + 1);
  stored_own_ip[2] = (uint8_t)*(p_dfu_own_ip + 2);
  stored_own_ip[3] = (uint8_t)*(p_dfu_own_ip + 3);

  if((stored_own_ip[0] != *p_own_ip_uint8) || (stored_own_ip[1] != (*p_own_ip_uint8 + 1)) || 
    (stored_own_ip[2] != (*p_own_ip_uint8 + 2)) || (stored_own_ip[3] != (*p_own_ip_uint8 + 3)))
  {
  
    uint32_t own_ip_uint32[4];

    own_ip_uint32[0] = (uint32_t)*p_own_ip_uint8;
    own_ip_uint32[1] = (uint32_t)*(p_own_ip_uint8 + 1);
    own_ip_uint32[2] = (uint32_t)*(p_own_ip_uint8 + 2);
    own_ip_uint32[3] = (uint32_t)*(p_own_ip_uint8 + 3);

    flash_operation_t flash_parameters;

    flash_parameters.type = FLASH_OP_TYPE_WRITE;

    flash_parameters.params.write.p_start_addr = p_dfu_own_ip;
    flash_parameters.params.write.length = 16;
    flash_parameters.params.write.p_data = own_ip_uint32;

    const flash_operation_t * p_flash_parameters = &flash_parameters;
  
    ERROR_CHECK(mesh_flash_op_push(MESH_FLASH_USER_APP, p_flash_parameters, NULL));

//    __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Successfully written own IP to flash\n");

  }
}

void dfu_write_server_ip(uint8_t * p_server_ip_uint8)
{
  uint32_t * p_dfu_server_ip = (uint32_t *)0xFE014;

  uint8_t stored_server_ip[4];

  stored_server_ip[0] = (uint8_t)*p_dfu_server_ip;
  stored_server_ip[1] = (uint8_t)*(p_dfu_server_ip + 1);
  stored_server_ip[2] = (uint8_t)*(p_dfu_server_ip + 2);
  stored_server_ip[3] = (uint8_t)*(p_dfu_server_ip + 3);

  if((stored_server_ip[0] != *p_server_ip_uint8) || (stored_server_ip[1] != (*p_server_ip_uint8 + 1)) || 
    (stored_server_ip[2] != (*p_server_ip_uint8 + 2)) || (stored_server_ip[3] != (*p_server_ip_uint8 + 3)))
  {
  
  uint32_t server_ip_uint32[4];

  server_ip_uint32[0] = (uint32_t)*p_server_ip_uint8;
  server_ip_uint32[1] = (uint32_t)*(p_server_ip_uint8 + 1);
  server_ip_uint32[2] = (uint32_t)*(p_server_ip_uint8 + 2);
  server_ip_uint32[3] = (uint32_t)*(p_server_ip_uint8 + 3);

  flash_operation_t flash_parameters;

  flash_parameters.type = FLASH_OP_TYPE_WRITE;

  flash_parameters.params.write.p_start_addr = p_dfu_server_ip;
  flash_parameters.params.write.length = 16;
  flash_parameters.params.write.p_data = server_ip_uint32;

  const flash_operation_t * p_flash_parameters = &flash_parameters;
  
  ERROR_CHECK(mesh_flash_op_push(MESH_FLASH_USER_APP, p_flash_parameters, NULL));

//  __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Successfully written server IP to flash\n");

  }
}

bool get_dfu_flag()
{
  return dfu_button_flag;
}