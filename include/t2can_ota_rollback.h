#pragma once
#ifdef ESP_PLATFORM
#include <esp_ota_ops.h>
#include <esp_log.h>

static const char *kOtaTag = "ota_rollback";

static bool otaIsFreshBoot() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    return (running != nullptr && boot != nullptr && running != boot);
}

static void otaCheckAndBoot() {
    if (otaIsFreshBoot()) {
        ESP_LOGW(kOtaTag, "Fresh OTA boot detected — running unvalidated firmware");
        ESP_LOGW(kOtaTag, "If otaMarkValid() is not called, bootloader will roll back");
    }
}

static void otaMarkValid() {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(kOtaTag, "OTA firmware validated — rollback cancelled");
    } else if (err == ESP_FAIL) {
        // Not a fresh OTA boot — already validated or normal boot, nothing to do
        ESP_LOGD(kOtaTag, "No pending OTA validation (normal boot)");
    } else {
        ESP_LOGE(kOtaTag, "OTA mark valid failed: %s", esp_err_to_name(err));
    }
}

#endif // ESP_PLATFORM
