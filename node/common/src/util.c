#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"


// Returns true if arr1[0..n-1] and arr2[0..m-1]
// contain same elements.
bool arrays_are_equal(uint8_t * p_arr1, uint8_t * p_arr2, uint8_t n, uint8_t m)
{
    if (n != m)
        return false;
 
    for (uint8_t i = 0; i < n; i++)
         if (p_arr1[i] != p_arr2[i])
            return false;
 
    return true;
}

bool IPs_are_equal(uint8_t * ip1, uint8_t * ip2)
{
    return arrays_are_equal(ip1, ip2, 4, 4);
}


int32_t dict_find_index(dict_t dict, const uint8_t *key) {
    for (int32_t i = 0; i < dict->len; i++) {
        if (!memcmp((uint8_t *)dict->entry[i].key, (const uint8_t *)key, 6)) {
            return i;
        }
    }
    return -1;
}

int32_t dict_find(dict_t dict, const uint8_t *key, int32_t def) {
    int32_t idx = dict_find_index(dict, key);
    return idx == -1 ? def : dict->entry[idx].value;
}

void dict_add(dict_t dict, const uint8_t *key, int32_t value) {
   int32_t idx = dict_find_index(dict, key);
   if (idx != -1) {
       dict->entry[idx].value = value;
       return;
   }
   if (dict->len == dict->cap) {
       dict->cap *= 2;
       dict->entry = realloc((void *)dict->entry, dict->cap * sizeof(dict_entry_t));
   }
   strncpy((char *)&(dict->entry[dict->len].key[0]), strdup((char *)key), 6);
   dict->entry[dict->len].value = value;
   dict->len++;
}

dict_t dict_new(void) {
    dict_s proto = {0, 10, malloc(10 * sizeof(dict_entry_t))};
    dict_t d = malloc(sizeof(dict_s));
    *d = proto;
    return d;
}

void dict_free(dict_t dict) {
    for (int32_t i = 0; i < dict->len; i++) {
        free((void *)dict->entry[i].key);
    }
    free(dict->entry);
    free(dict);
}

