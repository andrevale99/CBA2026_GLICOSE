#ifndef SPIFFS_MANAGER_H
#define SPIFFS_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t spiffs_init(void);

esp_err_t spiffs_format(void);

esp_err_t spiffs_read_file(const char *filename, char **buffer, size_t *length);

esp_err_t spiffs_write_file(const char *filename, const char *data, size_t length);

esp_err_t spiffs_delete_file(const char *filename);

bool spiffs_file_exists(const char *filename);

#endif
