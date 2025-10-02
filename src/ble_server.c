#include "ble_server.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include <string.h>

static const char *TAG = "BLE_SERVER";

#define GATTS_SERVICE_UUID   0x00FF
#define GATTS_CHAR_UUID      0xFF01
#define GATTS_CHAR2_UUID     0xFF02
#define GATTS_NUM_HANDLE     10

static uint16_t gatt_service_handle;
static esp_gatt_char_prop_t gatt_property = 0;
static uint16_t gatt_conn_id = 0xFFFF;
static esp_gatt_if_t gatt_if_for_notify = ESP_GATT_IF_NONE;

static esp_attr_value_t gatt_char_val = {
    .attr_max_len = 64,
    .attr_len     = 0,
    .attr_value   = NULL,
};

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        ESP_LOGI(TAG, "Advertising started");
        break;
    default:
        break;
    }
}

static void gatts_cb(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                     esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        ESP_LOGI(TAG, "Register event, start service");
        esp_ble_gap_set_device_name("ESP32-BLE-SERVER");

        esp_ble_adv_params_t adv_params = {
            .adv_int_min        = 0x20,
            .adv_int_max        = 0x40,
            .adv_type           = ADV_TYPE_IND,
            .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
            .channel_map        = ADV_CHNL_ALL,
            .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        };
        esp_ble_gap_start_advertising(&adv_params);

        esp_gatt_srvc_id_t service_id = {
            .is_primary = true,
            .id.inst_id = 0x00,
            .id.uuid.len = ESP_UUID_LEN_16,
            .id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID,
        };

        esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE);
        break;
    }
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "Service created");
        gatt_service_handle = param->create.service_handle;
        esp_ble_gatts_start_service(gatt_service_handle);

        esp_bt_uuid_t char_uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid = {.uuid16 = GATTS_CHAR_UUID},
        };
        gatt_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

        esp_ble_gatts_add_char(gatt_service_handle, &char_uuid,
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               gatt_property, &gatt_char_val, NULL);

        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(TAG, "Characteristic added, handle=%d", param->add_char.attr_handle);
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "Device connected");
        gatt_conn_id = param->connect.conn_id;
        gatt_if_for_notify = gatts_if;
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Device disconnected");
        gatt_conn_id = 0xFFFF;
        esp_ble_gap_start_advertising(NULL); // restart advertising
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "Write event, value: %.*s", param->write.len, param->write.value);
        if (strncmp((char*)param->write.value, "send_now", param->write.len) == 0) {
            ESP_LOGI(TAG, "Client requested send_now!");
            ble_server_notify_value("Now sending data...");
        }
        break;

    default:
        break;
    }
}

esp_err_t ble_server_init(void) {
    esp_err_t ret;

    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret) ESP_LOGW(TAG, "BT mem release classic failed: %s", esp_err_to_name(ret));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) return ret;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) return ret;

    ret = esp_bluedroid_init();
    if (ret) return ret;

    ret = esp_bluedroid_enable();
    if (ret) return ret;

    esp_ble_gap_register_callback(gap_cb);
    esp_ble_gatts_register_callback(gatts_cb);
    esp_ble_gatts_app_register(0);

    return ESP_OK;
}

void ble_server_notify_value(const char *value) {
    if (gatt_conn_id != 0xFFFF && gatt_if_for_notify != ESP_GATT_IF_NONE) {
        esp_ble_gatts_send_indicate(
            gatt_if_for_notify,
            gatt_conn_id,
            gatt_service_handle + 2, // handle offset of characteristic
            strlen(value),
            (uint8_t*)value,
            false
        );
    }
}
