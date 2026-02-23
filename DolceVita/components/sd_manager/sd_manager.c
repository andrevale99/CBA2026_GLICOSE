#include "sd_manager.h"

static const char *TAG = "sd_manager";

esp_err_t sd_init(sd_manager_config_t *sd)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        // .format_if_mount_failed = true,
        .format_if_mount_failed = false,
        .max_files = 2,
        .allocation_unit_size = 16 * 1024};
    
    const char mount_point[] = MOUNT_POINT;

    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    memcpy(&sd->host, &host, sizeof(sdmmc_host_t));

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = sd->mosi_pin,
        .miso_io_num = sd->miso_pin,
        .sclk_io_num = sd->sclk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(sd->host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = sd->cs_pin;
    slot_config.host_id = sd->host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &(sd->card));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, sd->card);

    return ESP_OK;
}

esp_err_t sd_deinit(sd_manager_config_t *sd)
{
    ESP_LOGI(TAG, "Unmounting filesystem");
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, sd->card);
    ESP_LOGI(TAG, "Filesystem unmounted");

    ESP_LOGI(TAG, "Deinitializing bus");
    spi_bus_free(sd->host.slot);
    ESP_LOGI(TAG, "Bus deinitialized");

    return ESP_OK;
}

esp_err_t sd_format(sd_manager_config_t *sd)
{
    return sdmmc_full_erase(sd->card);
}

esp_err_t sd_read_file(sd_manager_config_t *sd, const char *filename, char *buffer)
{
    ESP_LOGI(TAG, "Reading file %s", filename);
    sd->file = fopen(filename, "r");
    if (sd->file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    fgets(buffer, SD_MAX_BUFFER_SIZE, sd->file);
    fclose(sd->file);

    // strip newline
    char *pos = strchr(buffer, '\n');
    if (pos)
    {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", buffer);

    return ESP_OK;
}

esp_err_t sd_write_file(sd_manager_config_t *sd, const char *filename, const char *data)
{
    ESP_LOGI(TAG, "Opening file %s", filename);
    sd->file = fopen(filename, "a");
    if (sd->file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(sd->file, data);
    fclose(sd->file);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

esp_err_t sd_delete_file(sd_manager_config_t *sd, const char *filename)
{
    ESP_LOGI(TAG, "Deleting file %s", filename);
    if (remove(filename) != 0)
    {
        ESP_LOGE(TAG, "Failed to delete file");
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool sd_file_exists(sd_manager_config_t *sd, const char *filename)
{
    ESP_LOGI(TAG, "Checking if file %s exists", filename);
    sd->file = fopen(filename, "r");
    if (sd->file)
    {
        fclose(sd->file);
        return true;
    }
    return false;
}
