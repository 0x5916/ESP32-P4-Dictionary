#pragma once
#include <stdint.h>

#define MAX_WORD_LEN    16
#define MAX_SUGGESTIONS 10
#define MAX_EDIT_DIST   2

#define RECORD_SIZE     (MAX_WORD_LEN + 1 + 2) // word + length byte + freq rank

typedef struct {
    char keyword[MAX_WORD_LEN];
    uint8_t length;
    uint16_t freq_rank;   // 1 = most common, 65535 = rarest
} __attribute__((packed)) dict_word_entry_t;

typedef struct {
    char word[MAX_WORD_LEN];
    int  score;           // lower = better
} dict_suggestion_t;

typedef struct {
    char ipa[64];
    char pos[16];
    char definition[512];
    char definition_zh[256];
    char example[256];
} dict_sense_t;

typedef struct {
    char keyword[32];
    uint8_t sense_count;
    dict_sense_t senses[8];
} dict_entry_t;
