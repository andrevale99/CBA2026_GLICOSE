#include <stdio.h>
#include "wifi_lib.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "config.h"
#include "spiffs_manager.h"

static const char *TAG = "[WIFI_LIB]";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t wifi_event_group = NULL;
static wifi_event_cb_t user_event_cb = NULL;
static int current_cred_index = 0;
static const wifi_net_cred_t *cred_list_ptr = NULL;
static size_t cred_list_size = 0;
static int retry_count = 0;

wifi_config_t wifi_config = {0};

static bool auto_reconnect = true;
static bool wifi_initialized = false;

static void ensure_wifi_initialized(void)
{
    if (!wifi_initialized)
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        ESP_ERROR_CHECK(esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT()));
        wifi_initialized = true;
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {

        ESP_LOGI(TAG, "Wi-Fi started, attempting to connect to SSID: %s", cred_list_ptr[current_cred_index].ssid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {

        ESP_LOGW(TAG, "Disconnected from SSID: %s", cred_list_ptr[current_cred_index].ssid);
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);

        if (auto_reconnect && retry_count < WIFI_MAX_RETRIES)
        {
            retry_count++;

            ESP_LOGI(TAG, "Retrying (%d/%d)...", retry_count, WIFI_MAX_RETRIES);

            esp_wifi_connect();
        }
        else
        {
            retry_count = 0;
            current_cred_index++;
            if (current_cred_index < cred_list_size)
            {
                xEventGroupClearBits(wifi_event_group, WIFI_FAIL_BIT);
                wifi_config_t wifi_config = {0};
                strncpy((char *)wifi_config.sta.ssid, cred_list_ptr[current_cred_index].ssid, sizeof(wifi_config.sta.ssid));
                strncpy((char *)wifi_config.sta.password, cred_list_ptr[current_cred_index].password, sizeof(wifi_config.sta.password));
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

                ESP_LOGI(TAG, "Trying next network: %s", cred_list_ptr[current_cred_index].ssid);

                esp_wifi_connect();
            }
            else
            {
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);

                ESP_LOGE(TAG, "All networks failed.");
            }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        retry_count = 0;
    }

    if (user_event_cb)
    {
        user_event_cb(event_id, event_data);
    }
}

esp_err_t wifi_init_sta(const wifi_net_cred_t *cred_list, size_t list_size)
{
    if (!cred_list && list_size > 0)
    {
        ESP_LOGE(TAG, "Invalid credentials list");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    cred_list_ptr = cred_list;
    cred_list_size = list_size;
    current_cred_index = 0;
    retry_count = 0;

    wifi_event_group = xEventGroupCreate();

    ensure_wifi_initialized();

    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    strncpy((char *)wifi_config.sta.ssid, cred_list_ptr[0].ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, cred_list_ptr[0].password, sizeof(wifi_config.sta.password));

    if (strlen((char *)wifi_config.sta.password) == 0)
    {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    else
    {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi Station initialized");
    return ESP_OK;
}

esp_err_t wifi_init_ap(const wifi_net_cred_t cred)
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ensure_wifi_initialized();

    esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.ap.ssid, cred.ssid, sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char *)wifi_config.ap.password, cred.password, sizeof(wifi_config.ap.password) - 1);

    wifi_config.ap.ssid_len = strlen(cred.ssid);
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen(cred.password) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started. SSID:%s password:%s", cred.ssid, strlen(cred.password) > 0 ? cred.password : "(none)");

    return ESP_OK;
}

esp_err_t wifi_connect(void)
{
    esp_err_t err;

    if (wifi_event_group)
    {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    }

    ESP_LOGI(TAG, "Connecting to SSID: %s", (const char *)wifi_config.sta.ssid);

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_connect();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start connection: %s", esp_err_to_name(err));
        return err;
    }

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(100000));

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Successfully connected to WiFi");
        return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGW(TAG, "Connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

bool wifi_is_connected(void)
{
    if (wifi_event_group)
    {
        EventBits_t bits = xEventGroupGetBits(wifi_event_group);
        return (bits & WIFI_CONNECTED_BIT) != 0;
    }
    return false;
}

esp_err_t wifi_enable_auto_reconnect(bool enable)
{
    auto_reconnect = enable;
    return ESP_OK;
}

esp_err_t wifi_scan(wifi_ap_record_t *results, uint16_t *count)
{
    if (results == NULL || count == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false};

    err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_scan_get_ap_records(count, results);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Scan completed, found %d access points", *count);

    return ESP_OK;
}

esp_err_t wifi_get_ip(char *ip_str, size_t len)
{
    if (ip_str == NULL || len < 16)
        return ESP_ERR_INVALID_ARG;

    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL)
    {
        return ESP_FAIL;
    }
    esp_err_t err = esp_netif_get_ip_info(netif, &ip_info);
    if (err != ESP_OK)
    {
        return err;
    }

    snprintf(ip_str, len, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

esp_err_t wifi_get_mac(char *mac_out)
{
    if (mac_out == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t mac[6];
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK)
        return err;

    sprintf(mac_out, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return ESP_OK;
}

esp_err_t wifi_save_credential(const char *ssid, const char *password)
{
    if (!ssid || !password)
        return ESP_ERR_INVALID_ARG;

    FILE *file = fopen(WIFI_CRED_FILE, "r");
    char *lines[WIFI_CRED_MAX_NUM] = {0};
    size_t count = 0;
    bool updated = false;

    if (file)
    {
        char buffer[WIFI_CRED_MAX_LINE];
        while (fgets(buffer, sizeof(buffer), file) && count < WIFI_CRED_MAX_NUM)
        {
            char stored_ssid[64], stored_pass[64];
            if (sscanf(buffer, "%63[^,],%63[^\n]", stored_ssid, stored_pass) == 2)
            {
                if (strcmp(stored_ssid, ssid) == 0)
                {
                    ESP_LOGW(TAG, "SSID matched existing entry, updating password.");
                    snprintf(buffer, sizeof(buffer), "%s,%s\n", ssid, password);
                    updated = true;
                }

                bool duplicate = false;
                for (size_t i = 0; i < count; i++)
                {
                    char line_ssid[64];
                    sscanf(lines[i], "%63[^,]", line_ssid);
                    if (strcmp(line_ssid, stored_ssid) == 0)
                    {
                        ESP_LOGW(TAG, "Duplicate SSID found in buffer, ignoring line: %s", stored_ssid);
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                {
                    lines[count++] = strdup(buffer);
                }
            }
        }
        fclose(file);
    }

    if (!updated)
    {
        if (count < WIFI_CRED_MAX_NUM)
        {
            char new_line[WIFI_CRED_MAX_LINE];
            if (password == NULL || strlen(password) == 0)
            {
                snprintf(new_line, sizeof(new_line), "%s,\n", ssid);
            }
            else
            {
                snprintf(new_line, sizeof(new_line), "%s,%s\n", ssid, password);
            }
            lines[count++] = strdup(new_line);
            ESP_LOGI(TAG, "Added new credential: %s", new_line);
            updated = true;
        }
        else
        {
            ESP_LOGE(TAG, "Max number of Wi-Fi credentials reached!");
            for (size_t i = 0; i < count; i++)
                free(lines[i]);
            return ESP_FAIL;
        }
    }

    file = fopen(WIFI_CRED_FILE, "w");
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open credentials file for writing.");
        for (size_t i = 0; i < count; i++)
            free(lines[i]);
        return ESP_FAIL;
    }

    for (size_t i = 0; i < count; i++)
    {
        fputs(lines[i], file);
        free(lines[i]);
    }
    fclose(file);

    ESP_LOGI(TAG, "Credential updated or added successfully (SSID: %s)", ssid);
    return ESP_OK;
}

esp_err_t wifi_load_credentials(wifi_net_cred_t *cred_list, size_t max_count, size_t *out_count)
{
    if (!cred_list || max_count == 0 || !out_count)
        return ESP_ERR_INVALID_ARG;

    FILE *file = fopen(WIFI_CRED_FILE, "r");
    if (!file)
    {
        ESP_LOGW(TAG, "No Wi-Fi credentials file found");
        *out_count = 0;
        return ESP_FAIL;
    }

    size_t count = 0;
    char line[WIFI_CRED_MAX_LINE];

    while (fgets(line, sizeof(line), file) && count < max_count)
    {
        char stored_ssid[64] = {0};
        char stored_pass[64] = {0};

        int n = sscanf(line, "%63[^,],%63[^\n]", stored_ssid, stored_pass);
        if (n >= 1)
        {

            stored_ssid[strcspn(stored_ssid, "\r\n")] = 0;
            stored_pass[strcspn(stored_pass, "\r\n")] = 0;

            strncpy(cred_list[count].ssid, stored_ssid, sizeof(cred_list[count].ssid) - 1);
            cred_list[count].ssid[sizeof(cred_list[count].ssid) - 1] = '\0';

            strncpy(cred_list[count].password, stored_pass, sizeof(cred_list[count].password) - 1);
            cred_list[count].password[sizeof(cred_list[count].password) - 1] = '\0';

            ESP_LOGI(TAG, "Loaded credential (SSID: %s) %s",
                     stored_ssid,
                     strlen(stored_pass) == 0 ? "(open network)" : "(secured)");
            count++;
        }
    }

    fclose(file);
    *out_count = count;

    ESP_LOGI(TAG, "Loaded %d Wi-Fi credentials", (int)count);
    return (count > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t wifi_clear_credentials(void)
{
    if (remove(WIFI_CRED_FILE) == 0)
    {
        ESP_LOGI(TAG, "Wi-Fi credentials file deleted successfully");
        return ESP_OK;
    }
    else
    {
        ESP_LOGW(TAG, "No Wi-Fi credentials file to delete or deletion failed");
        return ESP_FAIL;
    }
}

esp_err_t wifi_stop(void)
{
    if (!wifi_initialized)
        return ESP_OK;

    esp_err_t err;

    err = esp_wifi_stop();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop Wi-Fi: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister Wi-Fi event handler: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister IP event handler: %s", esp_err_to_name(err));
        return err;
    }

    if (wifi_event_group)
    {
        vEventGroupDelete(wifi_event_group);
        wifi_event_group = NULL;
    }

    wifi_initialized = false;
    cred_list_ptr = NULL;
    cred_list_size = 0;
    current_cred_index = 0;
    retry_count = 0;

    ESP_LOGI(TAG, "Wi-Fi stopped");
    return ESP_OK;
}

esp_err_t wifi_disconnect(void)
{
    if (!wifi_initialized)
        return ESP_OK;

    esp_err_t err;

    err = esp_wifi_disconnect();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to disconnect Wi-Fi: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Wi-Fi disconnected");
    return ESP_OK;
}