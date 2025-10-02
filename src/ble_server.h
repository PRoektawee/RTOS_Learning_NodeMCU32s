#pragma once
#include "esp_err.h"

esp_err_t ble_server_init(void);
void ble_server_notify_value(const char *value);