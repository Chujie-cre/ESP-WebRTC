idf_component_register(SRCS "app_main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES mqtt_client mqtt json driver esp_netif nvs_flash esp_event protocol_examples_common)
