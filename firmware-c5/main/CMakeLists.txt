idf_component_register(
    SRCS "main.c"
         "ocp_server.c"
         "ocp_frame.c"
         "ocp_transport.c"
         "status_led.c"
         "radio_arbiter.c"
         "wifi_recon.c"
         "beacon_parse.c"
         "${CMAKE_CURRENT_LIST_DIR}/../../protocol/ocp_text.c"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_LIST_DIR}/../../protocol"
    REQUIRES driver esp_timer nvs_flash app_update esp_wifi esp_event
)
