idf_component_register(SRCS "app_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES mqtt json driver esp_netif nvs_flash esp_event protocol_examples_common)
