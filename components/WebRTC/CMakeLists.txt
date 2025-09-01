# idf_component_register(
#     SRCS 
#         "webrtc_client.cpp" "esp-rtc.cpp"
#     INCLUDE_DIRS 
#         "."
#     REQUIRES 
#         esp_peer
#         esp_wifi
#         esp_event
#         nvs_flash
#         driver
#         freertos
#         lwip
# )