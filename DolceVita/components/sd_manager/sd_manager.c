#include "sd_manager.h"

static const char *TAG = "sd_manager";

esp_err_t sd_init(void)
{
    return ESP_OK;
}

esp_err_t sd_format(void)
{
    return ESP_OK;
}

esp_err_t sd_read_file(const char *filename, char **buffer, size_t *length)
{
    return ESP_OK;
}

esp_err_t sd_write_file(const char *filename, const char *data, size_t length)
{
    ESP_LOGI(TAG, "Opening file %s", filename);
    FILE *f = fopen(filename, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

esp_err_t sd_delete_file(const char *filename)
{
    return ESP_OK;
}

bool sd_file_exists(const char *filename)
{
    return false;
}
