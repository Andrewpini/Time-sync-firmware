#include "mesh_app_utils.h"

//  __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "HEI\r\n");

static uint32_t dfu_clear_flag()
{
  uint32_t error_code_dfu = sd_power_gpregret_clr(0, 0xFF);

  return error_code_dfu;
}

static void dfu_initiate_and_reset()
{ 
    ERROR_CHECK(sd_power_gpregret_set(0, 0xAB));

    uint32_t gpreget_info = 0x00;
		
    sd_power_gpregret_get(0, &gpreget_info);
    
    while(gpreget_info != 0xAB)
    {
      sd_power_gpregret_get(0, &gpreget_info);
    }

    ERROR_CHECK(sd_nvic_SystemReset());
}