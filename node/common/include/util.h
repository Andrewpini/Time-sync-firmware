
#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

bool arrays_are_equal(uint8_t * p_arr1, uint8_t * p_arr2, uint8_t n, uint8_t m);

bool mac_addresses_are_equal(uint8_t * ip1, uint8_t * ip2);

bool ip_addresses_are_equal(uint8_t * ip1, uint8_t * ip2);

#endif