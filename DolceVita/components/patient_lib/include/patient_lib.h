/**
 * @file patient_lib.h
 * @author José Igo
 * @brief Library for managing patient data with integrity checks and flexible storage backends.
 * @version 0.1
 * @date 2026-02-20
 *
 * @details
 * This library provides an abstraction layer to store, retrieve, and delete
 * patient data using pluggable storage backends. Data integrity is ensured
 * through SHA-256 hashing.
 *
 * The storage backend must implement the @ref storage_driver_t interface.
 *
 * @note Designed for embedded systems using ESP-IDF.
 *
 * @copyright Copyright (c) 2026
 */

#ifndef PATIENT_H
#define PATIENT_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Patient data structure.
 *
 * This structure holds the patient identification, associated sensor
 * information, and an integrity hash.
 */
typedef struct
{
    uint32_t id;            /**< Unique ID provided by the device */
    char sensor_serial[32]; /**< Sensor Serial Number string (null-terminated) */
    unsigned char hash[32]; /**< SHA-256 checksum for data integrity */
} patient_t;

/**
 * @brief Storage write function prototype.
 *
 * @param fn    File name or storage key.
 * @param data  Pointer to data buffer to be written.
 * @param size  Size of data in bytes.
 *
 * @return
 *      - ESP_OK on success
 *      - Appropriate error code otherwise
 */
typedef esp_err_t (*storage_write_fn)(const char *fn,
                                      const void *data,
                                      size_t size);

/**
 * @brief Storage read function prototype.
 *
 * @param fn    File name or storage key.
 * @param buf   Pointer to buffer pointer that will receive allocated data.
 * @param size  Pointer to variable that will receive data size.
 *
 * @note The implementation is responsible for allocating memory for @p buf.
 *       The caller must free it after use.
 *
 * @return
 *      - ESP_OK on success
 *      - Appropriate error code otherwise
 */
typedef esp_err_t (*storage_read_fn)(const char *fn,
                                     void **buf,
                                     size_t *size);

/**
 * @brief Storage delete function prototype.
 *
 * @param fn File name or storage key to remove.
 *
 * @return
 *      - ESP_OK on success
 *      - Appropriate error code otherwise
 */
typedef esp_err_t (*storage_delete_fn)(const char *fn);

/**
 * @brief Storage driver interface.
 *
 * This structure groups function pointers that implement the storage backend.
 * It allows the patient library to be storage-agnostic (SPIFFS, NVS, SD, etc).
 */
typedef struct
{
    storage_write_fn write; /**< Write callback */
    storage_read_fn read;   /**< Read callback */
    storage_delete_fn del;  /**< Delete callback */
} storage_driver_t;

/**
 * @brief Save patient data.
 *
 * Computes the SHA-256 hash of the patient structure and stores it using
 * the provided storage driver.
 *
 * @param p       Pointer to patient structure.
 * @param driver  Storage driver implementation.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameters are invalid
 *      - Other error codes from storage backend
 */
esp_err_t patient_save(patient_t *p, const storage_driver_t *driver);

/**
 * @brief Load patient data and verify integrity.
 *
 * Reads patient data from storage and validates the SHA-256 checksum.
 *
 * @param id      Patient ID to load.
 * @param p       Pointer to structure that will receive the data.
 * @param driver  Storage driver implementation.
 *
 * @return
 *      - ESP_OK if data is valid
 *      - ESP_ERR_INVALID_CRC if hash verification fails
 *      - ESP_ERR_NOT_FOUND if patient does not exist
 *      - Other error codes from storage backend
 */
esp_err_t patient_load(uint32_t id,
                       patient_t *p,
                       const storage_driver_t *driver);

/**
 * @brief Delete patient data from storage.
 *
 * @param id      Patient ID to delete.
 * @param driver  Storage driver implementation.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_FOUND if patient does not exist
 *      - Other error codes from storage backend
 */
esp_err_t patient_delete(uint32_t id,
                         const storage_driver_t *driver);

#endif