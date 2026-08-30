/* SPDX-License-Identifier: MIT
 *
 * Secure OTA demo for the classic ESP32.
 *
 * Boot flow:
 *   1. If this is the first boot of a freshly updated image, run a self-test
 *      and confirm the image (or roll back to the previous one).
 *   2. Connect to Wi-Fi.
 *   3. Poll the HTTPS manifest and update when a newer signed image exists.
 */
#include <stdbool.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "ota_client.h"
#include "wifi.h"

static const char *TAG = "app";

/* Post-update self-test.
 *
 * A real product would also probe its peripherals here (sensor I2C/SPI, storage,
 * radio). The demo checks the two invariants it can verify without hardware:
 *   1. the running OTA partition really carries a valid app description, and
 *   2. NVS is readable and writable (a broken NVS is a common bad-update mode).
 *
 * Set CONFIG_APP_SELF_TEST_FAIL=y to force a failure and exercise the automatic
 * rollback path on a real board. */
static bool app_self_test(void)
{
#if CONFIG_APP_SELF_TEST_FAIL
    ESP_LOGE(TAG, "Self-test forced to fail (CONFIG_APP_SELF_TEST_FAIL)");
    return false;
#endif

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGE(TAG, "Self-test: no running partition");
        return false;
    }

    esp_app_desc_t running_desc;
    if (esp_ota_get_partition_description(running, &running_desc) != ESP_OK) {
        ESP_LOGE(TAG, "Self-test: running partition has no valid app description");
        return false;
    }
    if (running_desc.version[0] == '\0') {
        ESP_LOGE(TAG, "Self-test: empty app version");
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open("selftest", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Self-test: NVS open failed: %s", esp_err_to_name(err));
        return false;
    }

    uint32_t marker = 0;
    err = nvs_get_u32(handle, "marker", &marker);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        /* First boot after the update: create the marker once. */
        err = nvs_set_u32(handle, "marker", 0x5A5A5A5AU);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Self-test: NVS round-trip failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Self-test passed (app v%s, NVS ok)", running_desc.version);
    return true;
}

static void confirm_or_rollback_new_image(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(running, &state) != ESP_OK) {
        return;
    }
    if (state != ESP_OTA_IMG_PENDING_VERIFY) {
        return;
    }

    ESP_LOGW(TAG, "First boot of a new image - running self-test");
    if (app_self_test()) {
        ESP_LOGI(TAG, "Self-test passed, marking image valid");
        esp_ota_mark_app_valid_cancel_rollback();
    } else {
        ESP_LOGE(TAG, "Self-test failed, rolling back");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    confirm_or_rollback_new_image();

    const esp_app_desc_t *desc = esp_app_get_description();
    ESP_LOGI(TAG, "Running %s v%s", desc->project_name, desc->version);
#if CONFIG_SECURE_BOOT
    ESP_LOGI(TAG, "Build configuration: Secure Boot ENABLED");
#else
    ESP_LOGI(TAG, "Build configuration: Secure Boot disabled (development)");
#endif
#if CONFIG_SECURE_FLASH_ENC_ENABLED
    ESP_LOGI(TAG, "Build configuration: Flash Encryption ENABLED");
#else
    ESP_LOGI(TAG, "Build configuration: Flash Encryption disabled (development)");
#endif

    if (wifi_init_sta() != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi unavailable, skipping OTA check");
        return;
    }

    ota_config_t ota_config = {
        .manifest_url = CONFIG_OTA_MANIFEST_URL,
    };

    err = ota_client_check_and_update(&ota_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA check failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Running application v%s", desc->version);
}
