#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t sd_card_mount(void);
esp_err_t sd_card_unmount(void);
bool sd_card_is_mounted(void);

esp_err_t sd_card_read_file(const char *path, void *buffer, size_t max_size, size_t *out_size);
