/* SPDX-License-Identifier: MIT */
#pragma once

#include "esp_err.h"

/**
 * Initialise NVS/Wi-Fi station and block until an IP address is obtained or
 * the retry budget is exhausted.
 */
esp_err_t wifi_init_sta(void);
