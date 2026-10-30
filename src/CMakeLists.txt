# P4-3CAN board: no on-chip WiFi radio. On esp32p4, swap the native
# esp_wifi component (headers only there, no radio behind them) for the
# esp_wifi_remote API shim, which speaks the same esp_wifi.h API over the
# onboard ESP32-C6 companion chip via esp_hosted. All other targets keep
# the native component and an identical build graph.
set(APP_WIFI_REQUIRES esp_wifi)
if(IDF_TARGET STREQUAL "esp32p4")
    set(APP_WIFI_REQUIRES espressif__esp_wifi_remote)
endif()

idf_component_register(
    SRCS
        "main.cpp"
        "espidf_runtime.cpp"
    INCLUDE_DIRS
        "../include"
        ".."
    REQUIRES
        app_update
        esp_http_client
        esp_http_server
        esp_https_ota
        esp_netif
        ${APP_WIFI_REQUIRES}
        nvs_flash
        spi_flash
        spiffs
        driver
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
