idf_component_register(SRCS "rtc_task.c" "system.c" "ble_task.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver config)

target_compile_options(${COMPONENT_LIB} PUBLIC -std=gnu++11)