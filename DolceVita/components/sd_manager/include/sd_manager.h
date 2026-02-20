#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <stdio.h>

#include "sdmmc_cmd.h"

#include "esp_err.h"
#include "esp_log.h"

typedef struct {

} sd_manager_config_t;

esp_err_t sd_init(void);

esp_err_t sd_format(void);

esp_err_t sd_read_file(const char *filename, char **buffer, size_t *length);

esp_err_t sd_write_file(const char *filename, const char *data, size_t length);

esp_err_t sd_delete_file(const char *filename);

bool sd_file_exists(const char *filename);

#endif