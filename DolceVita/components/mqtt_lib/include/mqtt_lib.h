#ifndef MQTT_LIB_H
#define MQTT_LIB_H

/**
 * @file mqtt_lib.h
 * @brief MQTT wrapper module for ESP32 using ESP-IDF's esp-mqtt component.
 *
 * This module provides a simple API to connect, publish, subscribe and handle
 * MQTT events through user-defined callbacks.
 */

#include "mqtt_client.h"
#include "esp_err.h"

/**
 * @typedef mqtt_connected_cb_t
 * @brief Callback type for MQTT connection event.
 *
 * This callback is invoked when the ESP32 successfully connects to the broker.
 */
typedef void (*mqtt_connected_cb_t)(void);

/**
 * @typedef mqtt_disconnected_cb_t
 * @brief Callback type for MQTT disconnection event.
 *
 * This callback is invoked when the ESP32 disconnects from the broker.
 */
typedef void (*mqtt_disconnected_cb_t)(void);

/**
 * @typedef mqtt_message_cb_t
 * @brief Callback type for incoming MQTT messages.
 *
 * @param topic Pointer to the received topic string.
 * @param data Pointer to the received message payload.
 * @param len Length of the message payload.
 */
typedef void (*mqtt_message_cb_t)(const char *topic, size_t topic_len, const uint8_t *payload, size_t payload_len);

/**
 * @struct mqtt_callbacks_t
 * @brief Set of user-defined callbacks for MQTT events.
 */
typedef struct
{
    mqtt_connected_cb_t on_connected;
    mqtt_disconnected_cb_t on_disconnected;
    mqtt_message_cb_t on_message;
} mqtt_callbacks_t;

/**
 * @brief Initialize and start the MQTT client.
 *
 * @param uri Broker URI (e.g., "mqtt://broker.hivemq.com").
 * @param callbacks Pointer to a structure with user-defined callbacks.
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if uri or callbacks is NULL
 *  - ESP_FAIL if client initialization fails
 */
esp_err_t mqtt_init(const char *uri, const mqtt_callbacks_t *callbacks);

/**
 * @brief Publish a message to a given topic.
 *
 * @param topic Topic string.
 * @param data Message payload.
 * @param qos Quality of Service level (0, 1, or 2).
 * @param retain Retain flag (0 = not retained, 1 = retained).
 * @return
 *  - ESP_OK on success
 *  - ESP_FAIL if client is not initialized or publish fails
 */

/**
 * @brief Start the MQTT client.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_FAIL if client is not initialized or start fails
 */
esp_err_t mqtt_start(void);

/**
 * @brief Publish a message to a given topic.
 *
 * @param topic Topic string.
 * @param data Message payload.
 * @param qos Quality of Service level (0, 1, or 2).
 * @param retain Retain flag (0 = not retained, 1 = retained).
 * @return
 *  - ESP_OK on success
 *  - ESP_FAIL if client is not initialized or publish fails
 */
esp_err_t mqtt_publish(const char *topic, const char *data, int len, int qos, int retain);

/**
 * @brief Publish binary data as Base64 to a given topic.
 *
 * This function encodes the provided binary data into Base64 format
 * before publishing it to the specified MQTT topic.
 *
 * @param topic Topic string.
 * @param data Pointer to the binary data to be published.
 * @param len Length of the binary data in bytes.
 * @param qos Quality of Service level (0, 1, or 2).
 * @param retain Retain flag (0 = not retained, 1 = retained).
 * @return
 *  - ESP_OK on success
 *  - ESP_FAIL if client is not initialized or publish fails
 *  - ESP_ERR_NO_MEM if memory allocation for Base64 encoding fails
 */
esp_err_t mqtt_publish_base64(const char *topic, const uint8_t *data, int len, int qos, int retain);
/**
 * @brief Subscribe to a given topic.
 *
 * @param topic Topic string.
 * @param qos Quality of Service level (0, 1, or 2).
 * @return
 *  - ESP_OK on success
 *  - ESP_FAIL if client is not initialized or subscribe fails
 */
esp_err_t mqtt_subscribe(const char *topic, int qos);

/**
 * @brief Check if the MQTT client is currently connected.
 *
 * @return true if connected, false otherwise.
 */
bool mqtt_is_connected(void);
/**
 * @brief Stop and destroy the MQTT client instance.
 *
 * This function should be called before shutting down the application
 * or if MQTT connection is no longer needed.
 */
void mqtt_stop(void);

#endif // MQTT_LIB_H
