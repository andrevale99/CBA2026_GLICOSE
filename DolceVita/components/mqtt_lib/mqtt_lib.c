#include "mqtt_lib.h"
#include "esp_log.h"
#include "config.h"
#include "mbedtls/base64.h"

static const char *TAG = "[MQTT_LIB]";
static esp_mqtt_client_handle_t client = NULL;
static mqtt_callbacks_t g_callbacks;
static const char *g_broker_uri = NULL;
static bool mqtt_connected = false;

static void mqtt_event_handler_cb(void *handler_args,
                                  esp_event_base_t base,
                                  int32_t event_id,
                                  void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        if (g_broker_uri)
        {
            ESP_LOGI(TAG, "Connected to broker: %s", g_broker_uri);
        }
        else
        {
            ESP_LOGI(TAG, "Connected to broker (URI not available)");
        }
        if (g_callbacks.on_connected)
        {
            g_callbacks.on_connected();
        }
        mqtt_connected = true;
        break;

    case MQTT_EVENT_DISCONNECTED:
        if (g_broker_uri)
        {
            ESP_LOGW(TAG, "Disconnected from broker: %s", g_broker_uri);
        }
        else
        {

            ESP_LOGW(TAG, "Disconnected from broker (URI not available)");
        }
        if (g_callbacks.on_disconnected)
            g_callbacks.on_disconnected();
        mqtt_connected = false;
        break;

    case MQTT_EVENT_DATA:
    {
        size_t decoded_len = 0;
        uint8_t *decoded_buf = NULL;

        int ret = mbedtls_base64_decode(NULL, 0, &decoded_len, (const uint8_t *)event->data, event->data_len);
        if (ret != 0 && ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL)
        {
            ESP_LOGW(TAG, "Base64 decode size query failed, passing raw payload");
            g_callbacks.on_message(event->topic, event->topic_len, (const uint8_t *)event->data, event->data_len);
            break;
        }

        decoded_buf = malloc(decoded_len);
        if (!decoded_buf)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for Base64 decoding");
            break;
        }

        if (mbedtls_base64_decode(decoded_buf, decoded_len, &decoded_len, (const uint8_t *)event->data, event->data_len) != 0)
        {
            ESP_LOGW(TAG, "Base64 decoding failed, passing raw payload");
            free(decoded_buf);
            g_callbacks.on_message(event->topic, event->topic_len, (const uint8_t *)event->data, event->data_len);
            break;
        }

        ESP_LOGI(TAG, "Base64 decoding successful");
        g_callbacks.on_message(event->topic, event->topic_len, decoded_buf, decoded_len);
        free(decoded_buf);
    }
    break;

    default:
        ESP_LOGD(TAG, "Unhandled event id: %d", event->event_id);
        break;
    }
}

esp_err_t mqtt_init(const char *uri, const mqtt_callbacks_t *callbacks)
{
    esp_err_t err;
    if (!uri || !callbacks)
        return ESP_ERR_INVALID_ARG;

    g_callbacks = *callbacks;
    g_broker_uri = uri;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .broker.address.port = MQTT_BROKER_PORT,
        .credentials.username = MQTT_BROKER_USERNAME,
        .credentials.authentication.password = MQTT_BROKER_KEY,
        .buffer.size = MQTT_BUFFER_SIZE_LEN,
        .session.keepalive = MQTT_KEEPALIVE_INTERVAL,

        .broker.verification.skip_cert_common_name_check = true,
        .broker.verification.certificate = NULL,
        .broker.verification.use_global_ca_store = false,

#ifdef MQTT_RECONNECT_ENABLED
        .network.reconnect_timeout_ms = MQTT_RECONNECT_TIMEOUT_MS,
        .network.disable_auto_reconnect = false,
#endif
#ifdef MQTT_LAST_WILL_ENABLED
        .session.last_will.topic = MQTT_LAST_WILL_TOPIC,
        .session.last_will.msg = MQTT_LAST_WILL_MSG,
        .session.last_will.qos = MQTT_LAST_WILL_QOS,
        .session.last_will.retain = MQTT_LAST_WILL_RETAIN,
#endif
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client)
        return ESP_FAIL;

    err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler_cb, client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register MQTT event handler");
        return err;
    }

    return ESP_OK;
}

esp_err_t mqtt_start(void)
{
    if (!client)
    {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_FAIL;
    }
    return esp_mqtt_client_start(client);
}

esp_err_t mqtt_publish(const char *topic, const char *data, int len, int qos, int retain)
{
    if (!client)
    {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(client, topic, data, len, qos, retain);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_base64(const char *topic, const uint8_t *data, int len, int qos, int retain)
{
    if (!client)
    {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_FAIL;
    }

    // calcula tamanho necessário para Base64
    size_t b64_len = 0;
    mbedtls_base64_encode(NULL, 0, &b64_len, data, len); // apenas retorna o tamanho

    uint8_t *b64_buf = malloc(b64_len + 1); // +1 para null-terminator
    if (!b64_buf)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for Base64");
        return ESP_ERR_NO_MEM;
    }

    if (mbedtls_base64_encode(b64_buf, b64_len, &b64_len, data, len) != 0)
    {
        ESP_LOGE(TAG, "Base64 encoding failed");
        free(b64_buf);
        return ESP_FAIL;
    }

    b64_buf[b64_len] = '\0'; // string segura

    int msg_id = esp_mqtt_client_publish(client, topic, (const char *)b64_buf, b64_len, qos, retain);

    free(b64_buf);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_subscribe(const char *topic, int qos)
{
    if (!client)
    {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_FAIL;
    }
    int msg_id = esp_mqtt_client_subscribe(client, topic, qos);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

bool mqtt_is_connected(void)
{
    if (!client)
    {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return false;
    }
    return mqtt_connected;
}

void mqtt_stop(void)
{
    if (client)
    {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
        mqtt_connected = false;
    }
}
