idf_component_register(SRCS "wifi_lib.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash config spiffs_manager )
