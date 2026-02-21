#include "patient.h"
#include "mbedtls/sha256.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "[PATIENT_LIB]";

/**
 * @brief Compute SHA-256 hash of the patient structure.
 *
 * @param[in]  p       Pointer to constant patient structure.
 * @param[out] output  Buffer (32 bytes) that receives the SHA-256 digest.
 *
 * @retval - ESP_OK               Hash computed successfully.
 * @retval - ESP_ERR_INVALID_ARG  Null pointer provided.
 * @retval - ESP_FAIL             mbedTLS internal failure.
 */
static esp_err_t compute_sha256(const patient_t *p, unsigned char *output)
{
    if (!p || !output)
        return ESP_ERR_INVALID_ARG;

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    if (mbedtls_sha256_starts(&ctx, 0) != 0)
    {
        mbedtls_sha256_free(&ctx);
        return ESP_FAIL;
    }

    if (mbedtls_sha256_update(&ctx, (const unsigned char *)p, offsetof(patient_t, hash)) != 0)
    {
        mbedtls_sha256_free(&ctx);
        return ESP_FAIL;
    }

    if (mbedtls_sha256_finish(&ctx, output) != 0)
    {
        mbedtls_sha256_free(&ctx);
        return ESP_FAIL;
    }

    mbedtls_sha256_free(&ctx);
    return ESP_OK;
}

esp_err_t patient_save(patient_t *p, const storage_driver_t *driver)
{
    if (!p || !driver || !driver->write || !driver->read)
        return ESP_ERR_INVALID_ARG;

    char filename[32];
    snprintf(filename, sizeof(filename), "p_%lu.bin",
             (unsigned long)p->id);

    void *buf = NULL;
    size_t size = 0;

    esp_err_t err = driver->read(filename, &buf, &size);

    if (err == ESP_OK)
    {
        ESP_LOGW(TAG, "Patient ID %lu already exists, updating...",
                 (unsigned long)p->id);
        free(buf);
    }
    else if (err != ESP_ERR_NOT_FOUND)
    {
        return err;
    }
    if (compute_sha256(p, p->hash) != ESP_OK)
    {
        ESP_LOGE(TAG, "Hash computation failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Saving patient ID %lu...", (unsigned long)p->id);
    return driver->write(filename, (const char *)p, sizeof(patient_t));
}

esp_err_t patient_load(uint32_t id, patient_t *p, const storage_driver_t *driver)
{
    if (!p || !driver || !driver->read)
        return ESP_ERR_INVALID_ARG;

    char filename[32];
    snprintf(filename, sizeof(filename), "p_%lu.bin", (unsigned long)id);

    char *buffer = NULL;
    size_t size = 0;

    if (driver->read(filename, &buffer, &size) != ESP_OK)
    {
        return ESP_ERR_NOT_FOUND;
    }

    if (size < sizeof(patient_t))
    {
        ESP_LOGE(TAG, "Size mismatch for ID %lu. Expected %d, got %d",
                 (unsigned long)id, (int)sizeof(patient_t), (int)size);
        free(buffer);
        return ESP_ERR_INVALID_SIZE;
    }

    patient_t *temp = (patient_t *)buffer;
    unsigned char check[32];

    if (compute_sha256(temp, check) != ESP_OK || memcmp(check, temp->hash, 32) != 0)
    {
        ESP_LOGE(TAG, "SHA-256 check failed for ID %lu! Data corrupted.", (unsigned long)id);
        free(buffer);
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(p, temp, sizeof(patient_t));
    free(buffer);

    ESP_LOGI(TAG, "Patient ID %lu loaded and verified", (unsigned long)id);
    return ESP_OK;
}

esp_err_t patient_delete(uint32_t id, const storage_driver_t *driver)
{
    if (!driver || !driver->del)
        return ESP_ERR_INVALID_ARG;

    char filename[32];
    snprintf(filename, sizeof(filename), "p_%lu.bin", (unsigned long)id);

    return driver->del(filename);
}