idf_component_register(SRCS "NTP.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash esp_wifi esp_event esp_system lwip)
