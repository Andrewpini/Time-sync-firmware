#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"


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
   strncpy((uint8_t *)&(dict->entry[dict->len].key[0]), strdup(key), 6);
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