#include "WiFi.h"

static const char *TAG = "[WIFI]";

// Quantidade de tentativas para conexão
static int s_retry_num = 0;

// Função para tratamento dos eventos
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Evento Wi-Fi: WIFI_EVENT_STA_START");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_STOP:
            ESP_LOGI(TAG, "Evento Wi-Fi: WIFI_EVENT_STA_STOP");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Evento Wi-Fi: WIFI_EVENT_STA_CONNECTED");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Evento Wi-Fi: WIFI_EVENT_STA_DISCONNECTED");
            // Tentando reconectar
            if (s_retry_num < WIFI_MAX_RETRY_CONNECTION)
            {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGW(TAG, "Tentando se conectar %d", s_retry_num);
            }
            else
            {
                ESP_LOGE(TAG, "Falha na conexão");
            }
            break;
        default:
            break;
        }
    }
    
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
             // Verificando se o evento é referente ao IP
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Pegamos um IP " IPSTR, IP2STR(&event->ip_info.ip));

        s_retry_num = 0;
            break;
        
        default:
            break;
        }
    }
}

esp_err_t wifi_init_station(void)
{
    ESP_ERROR_CHECK(esp_netif_init());                // LwIP
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Event loop for event task
    esp_netif_create_default_wifi_sta();              // Cria LwIP para config station

    // Configurando o Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Manipulador de eventos WiFi
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // Configuração da estrutura do WiFi modo sta com as credenciais e autenticações
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };

    // Definição do modo station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // Configuração do Wifi a partir da estrutura previamente definida
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    // Start no WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Configuração e inicialização do Wi-Fi finalizado!!!");

    // Initialize WiFi here
    return ESP_OK;
}