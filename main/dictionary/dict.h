#pragma once
#include <stdint.h>

typedef struct {
    char keyword[64];
    uint32_t offset;
    uint16_t length;
    uint32_t frequency;
} __attribute__((packed)) dict_index_entry_t;

typedef struct {
    char ipa[64];
    char pos[16];
    char definition[512];
    char definition_zh[512];
    char example[256];
} dict_sense_t;

typedef struct {
    char keyword[64];
    uint8_t sense_count;
    dict_sense_t senses[8];
} __attribute__((packed)) dict_entry_t;
