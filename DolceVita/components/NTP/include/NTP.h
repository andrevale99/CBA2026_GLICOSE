/**
 * @file NTP.h
 * @brief Simple NTP synchronization library for ESP32.
 * @author José Igo
 * @date 2026-02-19
 */

#ifndef NTP_H
#define NTP_H

#include "esp_err.h"
#include "esp_netif_sntp.h"
#include <time.h>
#include <stdbool.h>

#define NTP_RETRY_COUNT 15

/**
 * @brief UTC offset enum: from -12 to +14 hours in 1-hour increments.
 * * The enum values are designed so that when we subtract 12, we get the actual hour offset.
 * Example:
 * - NTP_OFFSET_M3 = 9 (9 - 12 = -3 hours)
 * - NTP_OFFSET_P2 = 14 (14 - 12 = +2 hours)
 */

typedef enum
{
    NTP_OFFSET_M12, /**< UTC -12:00 */
    NTP_OFFSET_M11, /**< UTC -11:00 */
    NTP_OFFSET_M10, /**< UTC -10:00 */
    NTP_OFFSET_M9,  /**< UTC -09:00 */
    NTP_OFFSET_M8,  /**< UTC -08:00 */
    NTP_OFFSET_M7,  /**< UTC -07:00 */
    NTP_OFFSET_M6,  /**< UTC -06:00 */
    NTP_OFFSET_M5,  /**< UTC -05:00 */
    NTP_OFFSET_M4,  /**< UTC -04:00 */
    NTP_OFFSET_M3,  /**< UTC -03:00 (Brasilia Time) */
    NTP_OFFSET_M2,  /**< UTC -02:00 */
    NTP_OFFSET_M1,  /**< UTC -01:00 */
    NTP_OFFSET_UTC, /**< UTC 00:00 (Greenwich) */
    NTP_OFFSET_P1,  /**< UTC +01:00 */
    NTP_OFFSET_P2,  /**< UTC +02:00 */
    NTP_OFFSET_P3,  /**< UTC +03:00 */
    NTP_OFFSET_P4,  /**< UTC +04:00 */
    NTP_OFFSET_P5,  /**< UTC +05:00 */
    NTP_OFFSET_P6,  /**< UTC +06:00 */
    NTP_OFFSET_P7,  /**< UTC +07:00 */
    NTP_OFFSET_P8,  /**< UTC +08:00 */
    NTP_OFFSET_P9,  /**< UTC +09:00 */
    NTP_OFFSET_P10, /**< UTC +10:00 */
    NTP_OFFSET_P11, /**< UTC +11:00 */
    NTP_OFFSET_P12, /**< UTC +12:00 */
    NTP_OFFSET_P13, /**< UTC +13:00 */
    NTP_OFFSET_P14  /**< UTC +14:00 */
} ntp_utc_offset_t;

/**
 * @brief Configuration structure for the NTP service.
 * - `server`: NTP server address.
 * - `sync_timeout_ms`: Timeout for each synchronization attempt in milliseconds.
 * - `utc_offset`: Timezone offset from UTC using the `ntp_utc_offset_t` enum.
 * - `is_initialized`: Internal flag to track if the NTP service has been initialized.
 */
typedef struct
{
    const char *server;          /**< NTP server address */
    uint32_t sync_timeout_ms;    /**< Timeout for each sync attempt in milliseconds */
    ntp_utc_offset_t utc_offset; /**< Desired UTC offset from ntp_utc_offset_t */
    bool is_initialized;         /**< Internal flag to check if service is running */
} ntp_config_t;

/**
 * @brief Initializes the SNTP service with the provided configuration.
 * * @param[in,out] config Pointer to the ntp_config_t structure.
 * @return
 * - ESP_OK on success.
 * - ESP_ERR_INVALID_ARG if config or server is NULL.
 */
esp_err_t ntp_init(ntp_config_t *config);

/**
 * @brief Stops the SNTP service and updates the initialized flag.
 * * @param[in,out] config Pointer to the ntp_config_t structure.
 * @return
 * - ESP_OK on success.
 * - ESP_ERR_INVALID_STATE if service was not initialized.
 */
esp_err_t ntp_deinit(ntp_config_t *config);

/**
 * @brief Waits for the SNTP service to synchronize the system time.
 * * This function will block until the time is successfully synchronized or the retry limit is reached.
 *  * @param[in] config Pointer to the ntp_config_t structure with the desired configuration.
 * @return
 * - ESP_OK if time was successfully synchronized.
 * - ESP_ERR_INVALID_STATE if the NTP service is not initialized.
 */
esp_err_t ntp_wait_for_sync(ntp_config_t *config);

/**
 * @brief Retrieves the current time from the SNTP service and applies the configured UTC offset.
 * * This function will block until the time is successfully retrieved or a timeout occurs.
 * * The UTC offset is applied by setting the TZ environment variable before calling localtime_r.
 * @param[in] config Pointer to the ntp_config_t structure with the desired configuration.
 * @param[out] timeinfo Pointer to a struct tm where the obtained time will be stored.
 * @return
 * - ESP_OK on success.
 * - ESP_ERR_INVALID_ARG if config or timeinfo is NULL.
 * - ESP_ERR_TIMEOUT if waiting for sync times out.
 * - ESP_FAIL if the obtained time is invalid (e.g., still at Epoch).
 */
esp_err_t ntp_get_time(ntp_config_t *config, struct tm *timeinfo);

#endif