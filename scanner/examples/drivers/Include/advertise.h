
#include <stdbool.h>
#include <stdint.h>


void advertise_init(void);
void advertise_ble_channel_once(uint8_t channel);
void advertise_set_payload(uint8_t * p_data, uint8_t len);


