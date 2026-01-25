#include "ble_hid_mouse.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/bas/ble_svc_bas.h"

static const char *TAG = "ble_hid_mouse";

// Device name
#define DEVICE_NAME "Air Mouse"

// HID Report Map for a simple mouse
// Report ID 1: Mouse (Buttons, X, Y, Wheel)
static const uint8_t hid_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (1)
    0x29, 0x03,        //     Usage Maximum (3)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x03,        //     Report Count (3)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x05,        //     Report Size (5)
    0x81, 0x03,        //     Input (Constant)
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x06,        //     Input (Data, Variable, Relative)
    0xC0,              //   End Collection
    0xC0               // End Collection
};

// HID Service UUIDs
static const ble_uuid16_t hid_svc_uuid = BLE_UUID16_INIT(0x1812);
static const ble_uuid16_t hid_info_uuid = BLE_UUID16_INIT(0x2A4A);
static const ble_uuid16_t hid_report_map_uuid = BLE_UUID16_INIT(0x2A4B);
static const ble_uuid16_t hid_control_point_uuid = BLE_UUID16_INIT(0x2A4C);
static const ble_uuid16_t hid_report_uuid = BLE_UUID16_INIT(0x2A4D);
static const ble_uuid16_t hid_protocol_mode_uuid = BLE_UUID16_INIT(0x2A4E);

// HID Information value
static const uint8_t hid_info_value[] = {0x11, 0x01, 0x00, 0x02}; // v1.11, no remote wake, normally connectable

// Protocol mode (Report Protocol = 1)
static uint8_t protocol_mode = 1;

// Connection handle
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;

// Report characteristic value handle
static uint16_t report_val_handle = 0;

// Current button state
static uint8_t current_buttons = 0;

// Forward declarations
static int hid_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

// Report Reference Descriptor values
static const uint8_t report_ref_input[] = {0x01, 0x01}; // Report ID 1, Input

// GATT service definitions
static const struct ble_gatt_chr_def hid_characteristics[] = {
    {
        // HID Information
        .uuid = &hid_info_uuid.u,
        .access_cb = hid_chr_access,
        .flags = BLE_GATT_CHR_F_READ,
    },
    {
        // Report Map
        .uuid = &hid_report_map_uuid.u,
        .access_cb = hid_chr_access,
        .flags = BLE_GATT_CHR_F_READ,
    },
    {
        // HID Control Point
        .uuid = &hid_control_point_uuid.u,
        .access_cb = hid_chr_access,
        .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    {
        // Report (Input - Mouse)
        .uuid = &hid_report_uuid.u,
        .access_cb = hid_chr_access,
        .val_handle = &report_val_handle,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .descriptors = (struct ble_gatt_dsc_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(0x2908), // Report Reference
                .att_flags = BLE_ATT_F_READ,
                .access_cb = hid_chr_access,
            },
            {0}
        },
    },
    {
        // Protocol Mode
        .uuid = &hid_protocol_mode_uuid.u,
        .access_cb = hid_chr_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    {0}
};

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &hid_svc_uuid.u,
        .characteristics = hid_characteristics,
    },
    {0}
};

static int hid_chr_access(uint16_t conn_handle_arg, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;

    if (ble_uuid_cmp(uuid, &hid_info_uuid.u) == 0) {
        int rc = os_mbuf_append(ctxt->om, hid_info_value, sizeof(hid_info_value));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ble_uuid_cmp(uuid, &hid_report_map_uuid.u) == 0) {
        int rc = os_mbuf_append(ctxt->om, hid_report_map, sizeof(hid_report_map));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ble_uuid_cmp(uuid, &hid_protocol_mode_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            int rc = os_mbuf_append(ctxt->om, &protocol_mode, 1);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            if (OS_MBUF_PKTLEN(ctxt->om) == 1) {
                os_mbuf_copydata(ctxt->om, 0, 1, &protocol_mode);
            }
            return 0;
        }
    }

    if (ble_uuid_cmp(uuid, &hid_control_point_uuid.u) == 0) {
        // Ignore control point writes (suspend/exit suspend)
        return 0;
    }

    // Report Reference descriptor
    if (ctxt->dsc != NULL && ble_uuid_cmp(ctxt->dsc->uuid, BLE_UUID16_DECLARE(0x2908)) == 0) {
        int rc = os_mbuf_append(ctxt->om, report_ref_input, sizeof(report_ref_input));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    return 0;
}

static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));

    // Flags: General Discoverable, BR/EDR Not Supported
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Include complete local name
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;

    // Appearance: Mouse
    fields.appearance = 0x03C2;
    fields.appearance_is_present = 1;

    // Include HID service UUID
    fields.uuids16 = (ble_uuid16_t[]){BLE_UUID16_INIT(0x1812)};
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting adv fields: %d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertising: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Advertising started");
}

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "Connected, handle=%d", conn_handle);

            // Request connection parameter update for lower latency
            struct ble_gap_upd_params params = {
                .itvl_min = 6,  // 7.5ms
                .itvl_max = 12, // 15ms
                .latency = 0,
                .supervision_timeout = 200, // 2s
            };
            ble_gap_update_params(conn_handle, &params);
        } else {
            conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ble_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected, reason=%d", event->disconnect.reason);
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_advertise();
        break;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGI(TAG, "Connection updated");
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ble_advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Subscribe event, attr_handle=%d", event->subscribe.attr_handle);
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update, mtu=%d", event->mtu.value);
        break;
    }

    return 0;
}

static void ble_on_sync(void)
{
    int rc;

    // Set device name
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting device name: %d", rc);
    }

    // Set appearance to Mouse
    ble_svc_gap_device_appearance_set(0x03C2);

    // Start advertising
    ble_advertise();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset, reason=%d", reason);
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_hid_mouse_init(void)
{
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize NimBLE
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NimBLE: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize GAP and GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_bas_init();

    // Set initial battery level
    ble_svc_bas_battery_level_set(90);

    // Configure custom GATT services
    int rc = ble_gatts_count_cfg(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error counting GATT services: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error adding GATT services: %d", rc);
        return ESP_FAIL;
    }

    // Set callbacks
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    // Set security requirements for HID
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE HID Mouse initialized");
    return ESP_OK;
}

bool ble_hid_mouse_is_connected(void)
{
    return conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

static esp_err_t send_mouse_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }

    // Report format: Report ID (1), Buttons, X, Y, Wheel
    uint8_t report[4] = {buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel};

    struct os_mbuf *om = ble_hs_mbuf_from_flat(report, sizeof(report));
    if (om == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int rc = ble_gatts_notify_custom(conn_handle, report_val_handle, om);
    if (rc != 0) {
        ESP_LOGD(TAG, "Notify failed: %d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ble_hid_mouse_move(int8_t x, int8_t y)
{
    return send_mouse_report(current_buttons, x, y, 0);
}

esp_err_t ble_hid_mouse_move_wheel(int8_t x, int8_t y, int8_t wheel)
{
    return send_mouse_report(current_buttons, x, y, wheel);
}

esp_err_t ble_hid_mouse_press(uint8_t buttons)
{
    current_buttons |= buttons;
    return send_mouse_report(current_buttons, 0, 0, 0);
}

esp_err_t ble_hid_mouse_release(void)
{
    current_buttons = 0;
    return send_mouse_report(0, 0, 0, 0);
}

esp_err_t ble_hid_mouse_click(uint8_t buttons)
{
    esp_err_t ret = ble_hid_mouse_press(buttons);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(10));
    return ble_hid_mouse_release();
}

void ble_hid_mouse_set_battery_level(uint8_t level)
{
    ble_svc_bas_battery_level_set(level);
}
