#include <stdbool.h>

#include "mesh_app_utils.h"
#include "mesh_flash.h"

//  __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "HEI\r\n");

static bool dfu_button_flag = false;

void dfu_set_button_flag();

void dfu_clear_button_flag();

uint32_t dfu_clear_bootloader_flag();

void dfu_initiate_and_reset();

void dfu_button_initiate_and_reset(); // Not used now - will go into bootloader and into an spin lock until BUTTON 0 is pressed or the bootloader timeout timer kicks inn

void dfu_write_own_ip(uint8_t * p_own_ip_uint8);

void dfu_write_server_ip(uint8_t * p_server_ip_uint8);

bool get_dfu_flag();