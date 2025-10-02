#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ble_server.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "Init BLE server...");
    ble_server_init();

    while (1) {
        ESP_LOGI(TAG, "Main loop alive");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}