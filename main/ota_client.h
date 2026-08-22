/* SPDX-License-Identifier: MIT */
#pragma once

#include "esp_err.h"

typedef struct {
    /* Full HTTPS URL of the release manifest, e.g.
       https://example.com/manifest.json */
    const char *manifest_url;
} ota_config_t;

/**
 * Fetch the release manifest over HTTPS, compare its version with the running
 * image and, when the manifest advertises a newer version, download and apply
 * the referenced firmware image.
 *
 * The authenticity of the downloaded image is provided by Secure Boot v2
 * (RSA-PSS signature verified by the bootloader); the manifest itself is only
 * used for version gating and release metadata.
 *
 * @return ESP_OK when already up to date or after a successful update,
 *         an error code otherwise.
 */
esp_err_t ota_client_check_and_update(const ota_config_t *config);
