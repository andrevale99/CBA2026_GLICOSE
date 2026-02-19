#ifndef _WIFI_LIB_H
#define _WIFI_LIB_H

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        char ssid[32];
        char password[64];
    } wifi_net_cred_t;

    typedef void (*wifi_event_cb_t)(wifi_event_t event, void *arg);

    esp_err_t wifi_init_sta(const wifi_net_cred_t *cred_list, size_t list_size);

    esp_err_t wifi_init_ap(const wifi_net_cred_t cred);

    esp_err_t wifi_connect(void);

    bool wifi_is_connected(void);

    esp_err_t wifi_enable_auto_reconnect(bool enable);

    esp_err_t wifi_scan(wifi_ap_record_t *results, uint16_t *count);

    esp_err_t wifi_get_ip(char *ip_str, size_t len);

    esp_err_t wifi_get_mac(char *mac_out);

    esp_err_t wifi_stop(void);

    esp_err_t wifi_disconnect(void);

    esp_err_t wifi_save_credential(const char *ssid, const char *password);

    esp_err_t wifi_load_credentials(wifi_net_cred_t *cred_list, size_t max_count, size_t *out_count);

    esp_err_t wifi_clear_credentials(void);

#ifdef __cplusplus
}
#endif

#endif // _CONFIG_H
