#ifndef NTP_H
#define NTP_H

#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#define NTP_RETRY_COUNT 15

esp_err_t ntp_init(const char* ntp_server);
esp_err_t ntp_wait_for_sync(uint32_t timeout_ms);
void ntp_get_time(struct tm* timeinfo);

#endif
