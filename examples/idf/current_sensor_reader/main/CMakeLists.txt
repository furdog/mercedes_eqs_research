idf_component_register(SRCS "current_sensor_reader.c"
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS "../../../../src"
                    REQUIRES esp_driver_twai esp_timer)
