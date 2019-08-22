#include <stdbool.h>

#include "mesh_app_utils.h"
#include "mesh_flash.h"

uint32_t dfu_clear_bootloader_flag(void);

void dfu_initiate_and_reset(void);

void dfu_button_initiate_and_reset(void); // Not used now - will go into bootloader and into an spin lock until BUTTON 0 is pressed or the bootloader timeout timer kicks inn

void dfu_erase_flash_page(void);

void dfu_write_own_ip(uint8_t * p_own_ip_uint8);

void dfu_write_server_ip(uint8_t * p_server_ip_uint8);

void dfu_start(void);