/* SPDX-License-Identifier: MIT */
#include "ota_client.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota";

/* Embedded by EMBED_TXTFILES in main/CMakeLists.txt. */
extern const uint8_t server_ca_pem_start[] asm("_binary_server_ca_pem_start");
extern const uint8_t server_ca_pem_end[] asm("_binary_server_ca_pem_end");

#define MANIFEST_MAX_LEN 4096
#define FIRMWARE_URL_MAX_LEN 512
#define HTTP_TIMEOUT_MS 20000

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} http_buffer_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_buffer_t *buf = (http_buffer_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && buf != NULL) {
        if (buf->len + evt->data_len + 1 > buf->cap) {
            ESP_LOGE(TAG, "HTTP response exceeds %u bytes", (unsigned)buf->cap);
            return ESP_FAIL;
        }
        memcpy(buf->data + buf->len, evt->data, evt->data_len);
        buf->len += evt->data_len;
        buf->data[buf->len] = '\0';
    }
    return ESP_OK;
}

static esp_err_t fetch_text(const char *url, char *out, size_t out_cap)
{
    http_buffer_t buf = { .data = out, .len = 0, .cap = out_cap };
    out[0] = '\0';

    esp_http_client_config_t config = {
        .url = url,
        .cert_pem = (const char *)server_ca_pem_start,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .user_data = &buf,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GET %s failed: %s", url, esp_err_to_name(err));
        return err;
    }
    if (status != 200) {
        ESP_LOGE(TAG, "GET %s -> HTTP %d", url, status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* Very small "major.minor.patch" comparator. Returns >0 when a is newer. */
static int compare_versions(const char *a, const char *b)
{
    while (*a != '\0' || *b != '\0') {
        char *end_a = NULL;
        char *end_b = NULL;
        long va = strtol(a, &end_a, 10);
        long vb = strtol(b, &end_b, 10);
        if (va != vb) {
            return va > vb ? 1 : -1;
        }
        a = end_a;
        b = end_b;
        if (*a == '.') {
            a++;
        }
        if (*b == '.') {
            b++;
        }
    }
    return 0;
}

static void build_firmware_url(const char *manifest_url, const char *rel,
                               char *out, size_t out_cap)
{
    if (strncmp(rel, "http", 4) == 0) {
        snprintf(out, out_cap, "%s", rel);
        return;
    }
    const char *slash = strrchr(manifest_url, '/');
    size_t base_len = slash != NULL ? (size_t)(slash - manifest_url + 1)
                                    : strlen(manifest_url);
    snprintf(out, out_cap, "%.*s%s", (int)base_len, manifest_url, rel);
}

static esp_err_t perform_ota(const char *firmware_url)
{
    esp_http_client_config_t http_config = {
        .url = firmware_url,
        .cert_pem = (const char *)server_ca_pem_start,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    ESP_LOGI(TAG, "Starting OTA from %s", firmware_url);

    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }

    while (true) {
        err = esp_https_ota_perform(handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_perform failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(handle);
        return err;
    }

    if (!esp_https_ota_is_complete_data_received(handle)) {
        ESP_LOGE(TAG, "OTA download incomplete");
        esp_https_ota_abort(handle);
        return ESP_FAIL;
    }

    err = esp_https_ota_finish(handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed - signature rejected");
        } else {
            ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(err));
        }
        return err;
    }

    ESP_LOGI(TAG, "OTA applied successfully");
    return ESP_OK;
}

esp_err_t ota_client_check_and_update(const ota_config_t *config)
{
    if (config == NULL || config->manifest_url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char *manifest = malloc(MANIFEST_MAX_LEN);
    if (manifest == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = fetch_text(config->manifest_url, manifest, MANIFEST_MAX_LEN);
    if (err != ESP_OK) {
        free(manifest);
        return err;
    }

    cJSON *root = cJSON_Parse(manifest);
    free(manifest);
    if (root == NULL) {
        ESP_LOGE(TAG, "manifest is not valid JSON");
        return ESP_FAIL;
    }

    const cJSON *j_version = cJSON_GetObjectItemCaseSensitive(root, "version");
    const cJSON *j_firmware = cJSON_GetObjectItemCaseSensitive(root, "firmware");
    const cJSON *j_sha = cJSON_GetObjectItemCaseSensitive(root, "sha256");

    if (!cJSON_IsString(j_version) || !cJSON_IsString(j_firmware)) {
        ESP_LOGE(TAG, "manifest missing 'version' or 'firmware'");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    const esp_app_desc_t *running = esp_app_get_description();
    ESP_LOGI(TAG, "manifest version=%s running version=%s",
             j_version->valuestring, running->version);

    if (compare_versions(j_version->valuestring, running->version) <= 0) {
        ESP_LOGI(TAG, "Firmware already up to date");
        cJSON_Delete(root);
        return ESP_OK;
    }

    if (cJSON_IsString(j_sha)) {
        ESP_LOGI(TAG, "Target image sha256=%s", j_sha->valuestring);
    }

    char firmware_url[FIRMWARE_URL_MAX_LEN];
    build_firmware_url(config->manifest_url, j_firmware->valuestring,
                       firmware_url, sizeof(firmware_url));

    err = perform_ota(firmware_url);
    cJSON_Delete(root);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGW(TAG, "Restarting into the new image");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK; /* not reached */
}
