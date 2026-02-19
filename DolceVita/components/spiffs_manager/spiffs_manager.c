#include "spiffs_manager.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

static const char *TAG = "[SPIFFS_MANAGER]";

static void build_full_path(const char *filename, char *out_path, size_t size)
{
    snprintf(out_path, size, "%s/%s", SPIFFS_BASE_PATH, filename);
}

esp_err_t spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE_PATH,
        .partition_label = NULL,
        .max_files = SPIFFS_MAX_FILES,
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS mounted successfully: total=%d, used=%d", total, used);
    return ESP_OK;
}

esp_err_t spiffs_format(void)
{
    ESP_LOGI(TAG, "Formatting SPIFFS...");
    esp_err_t ret = esp_spiffs_format(NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to format SPIFFS (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "SPIFFS formatted successfully");
    }
    return ret;
}

esp_err_t spiffs_read_file(const char *filename, char **buffer, size_t *length)
{
    char full_path[SPIFFS_FULL_PATH_SIZE];
    build_full_path(filename, full_path, sizeof(full_path));
    ESP_LOGD(TAG, "Reading file: %s", full_path);

    FILE *f = fopen(full_path, "rb");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", full_path);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    *length = ftell(f);
    fseek(f, 0, SEEK_SET);

    *buffer = malloc(*length + 1);
    if (!*buffer)
    {
        ESP_LOGE(TAG, "Not enough memory to read file: %s", full_path);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(*buffer, 1, *length, f);
    (*buffer)[*length] = '\0';
    fclose(f);

    ESP_LOGI(TAG, "File read successfully: %s (%d bytes)", full_path, (int)*length);
    return ESP_OK;
}

esp_err_t spiffs_write_file(const char *filename, const char *data, size_t length)
{
    char full_path[SPIFFS_FULL_PATH_SIZE];
    build_full_path(filename, full_path, sizeof(full_path));
    ESP_LOGD(TAG, "Writing file: %s", full_path);

    FILE *f = fopen(full_path, "wb");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path);
        return ESP_FAIL;
    }

    fwrite(data, 1, length, f);
    fclose(f);

    ESP_LOGI(TAG, "File written successfully: %s (%d bytes)", full_path, (int)length);
    return ESP_OK;
}

esp_err_t spiffs_delete_file(const char *filename)
{
    char full_path[SPIFFS_FULL_PATH_SIZE];
    build_full_path(filename, full_path, sizeof(full_path));
    ESP_LOGD(TAG, "Deleting file: %s", full_path);

    if (remove(full_path) == 0)
    {
        ESP_LOGI(TAG, "File deleted successfully: %s", full_path);
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to delete file: %s", full_path);
        return ESP_FAIL;
    }
}

bool spiffs_file_exists(const char *filename)
{
    char full_path[SPIFFS_FULL_PATH_SIZE];
    build_full_path(filename, full_path, sizeof(full_path));

    FILE *f = fopen(full_path, "rb");
    if (f)
    {
        fclose(f);
        ESP_LOGD(TAG, "File exists: %s", full_path);
        return true;
    }
    ESP_LOGD(TAG, "File does not exist: %s", full_path);
    return false;
}
