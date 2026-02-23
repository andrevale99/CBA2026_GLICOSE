#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <stdio.h>

#include "sdmmc_cmd.h"

#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"

#define MOUNT_POINT "/sdcard"

#define SD_MAX_BUFFER_SIZE 1024

typedef struct {
    int mosi_pin;
    int miso_pin;
    int sclk_pin;
    int cs_pin;

    sdmmc_host_t host;

    sdmmc_card_t *card;

    FILE *file;

} sd_manager_config_t;

esp_err_t sd_init(sd_manager_config_t *config);

esp_err_t sd_format(void);

esp_err_t sd_read_file(sd_manager_config_t *config, const char *filename, char *buffer);

esp_err_t sd_write_file(sd_manager_config_t *config, const char *filename, const char *data);

esp_err_t sd_delete_file(sd_manager_config_t *config, const char *filename);

bool sd_file_exists(sd_manager_config_t *config, const char *filename);

#endif