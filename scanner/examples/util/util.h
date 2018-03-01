#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct dict_entry_s {
    const uint8_t key[6];
    int value;
} dict_entry_t;

typedef struct dict_s {
    int len;
    int cap;
    dict_entry_t *entry;
} dict_s, *dict_t;

int32_t dict_find_index(dict_t dict, const uint8_t *key);
int32_t dict_find(dict_t dict, const uint8_t *key, int32_t def);
void dict_add(dict_t dict, const uint8_t *key, int32_t value);
dict_t dict_new(void);
void dict_free(dict_t dict);



#endif	// UTIL_H