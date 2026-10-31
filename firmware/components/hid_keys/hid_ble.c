#include "sdkconfig.h"

#if CONFIG_AUTOLOCK_HID_BLE

#include <string.h>

#include "esp_check.h"
#include "esp_hidd.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hid_keys.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "hid_ble";

#define REPORT_ID_KEYBOARD   1
#define KEY_GAP_MS           30
#define APPEARANCE_KEYBOARD  0x03C1
#define HID_SERVICE_UUID     0x1812

void ble_store_config_init(void);

/* Standard 8-byte keyboard input report (modifiers, reserved, 6 keys) + LED output. */
static const uint8_t s_report_map[] = {
    0x05, 0x01,       /* Usage Page (Generic Desktop) */
    0x09, 0x06,       /* Usage (Keyboard) */
    0xA1, 0x01,       /* Collection (Application) */
    0x85, REPORT_ID_KEYBOARD,
    0x05, 0x07,       /*   Usage Page (Keyboard) */
    0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02,   /*   8 modifier bits */
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,   /*   reserved byte */
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
    0x95, 0x05, 0x75, 0x01, 0x91, 0x02,   /*   5 LED bits */
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01,   /*   LED padding */
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x15, 0x00, 0x25, 0x65,
    0x95, 0x06, 0x75, 0x08, 0x81, 0x00,   /*   6 key codes */
    0xC0,             /* End Collection */
};

static esp_hid_raw_report_map_t s_maps[] = {
    {.data = s_report_map, .len = sizeof(s_report_map)},
};

static esp_hid_device_config_t s_cfg = {
    .vendor_id = 0x303A,
    .product_id = 0x4004, /* Espressif development default, same as the USB build */
    .version = 0x0100,
    .device_name = "Radar Auto-Lock",
    .manufacturer_name = "Kymopsis",
    .serial_number = "000001",
    .report_maps = s_maps,
    .report_maps_len = 1,
};

static esp_hidd_dev_t *s_dev;
static volatile bool s_connected;

static void adv_start(void);

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    struct ble_gap_conn_desc desc;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connect %s", event->connect.status == 0 ? "ok" : "failed");
        if (event->connect.status != 0) {
            adv_start();
        }
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (!s_connected) {
            adv_start();
        }
        return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "encryption %s", event->enc_change.status == 0 ? "on" : "failed");
        return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* The computer lost its key (e.g. "Remove device"): accept a fresh pairing. */
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    default:
        return 0;
    }
}

static void adv_start(void)
{
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    static ble_uuid16_t hid_uuid = BLE_UUID16_INIT(HID_SERVICE_UUID);
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.appearance = APPEARANCE_KEYBOARD;
    fields.appearance_is_present = 1;
    fields.name = (uint8_t *)s_cfg.device_name;
    fields.name_len = (uint8_t)strlen(s_cfg.device_name);
    fields.name_is_complete = 1;
    fields.uuids16 = &hid_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv fields rc=%d", rc);
        return;
    }
    struct ble_gap_adv_params params;
    memset(&params, 0, sizeof(params));
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);
    params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "adv start rc=%d", rc);
    }
}

static void hidd_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    (void)base;
    (void)event_data;
    switch ((esp_hidd_event_t)id) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "advertising as \"%s\"", s_cfg.device_name);
        adv_start();
        break;
    case ESP_HIDD_CONNECT_EVENT:
        s_connected = true;
        ESP_LOGI(TAG, "computer connected");
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        s_connected = false;
        ESP_LOGI(TAG, "computer disconnected");
        adv_start();
        break;
    default:
        break;
    }
}

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static esp_err_t send_report(uint8_t modifiers, uint8_t keycode)
{
    uint8_t report[8] = {modifiers, 0, keycode, 0, 0, 0, 0, 0};
    return esp_hidd_dev_input_set(s_dev, 0, REPORT_ID_KEYBOARD, report, sizeof(report));
}

esp_err_t hid_keys_init(const char *device_name)
{
    if (device_name) {
        s_cfg.device_name = device_name;
    }
    ESP_RETURN_ON_ERROR(nimble_port_init(), TAG, "nimble init"); /* also starts the controller */

    /* "Just works" pairing with bonding: no PIN to type on a device without a keypad. */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ESP_RETURN_ON_ERROR(esp_hidd_dev_init(&s_cfg, ESP_HID_TRANSPORT_BLE, hidd_event, &s_dev), TAG,
                        "hidd init");
    ble_svc_gap_device_name_set(s_cfg.device_name);
    ble_svc_gap_device_appearance_set(APPEARANCE_KEYBOARD);
    ble_store_config_init();
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

hid_link_state_t hid_keys_state(void)
{
    return s_connected ? HID_LINK_READY : HID_LINK_DOWN;
}

esp_err_t hid_keys_combo(uint8_t modifiers, uint8_t keycode)
{
    ESP_RETURN_ON_FALSE(s_connected, ESP_ERR_INVALID_STATE, TAG, "not connected");
    esp_err_t err = send_report(modifiers, 0);
    vTaskDelay(pdMS_TO_TICKS(KEY_GAP_MS));
    if (err == ESP_OK) {
        err = send_report(modifiers, keycode);
        vTaskDelay(pdMS_TO_TICKS(KEY_GAP_MS));
    }
    esp_err_t rel = send_report(0, 0);
    return err != ESP_OK ? err : rel;
}

esp_err_t hid_keys_wake(void)
{
    return hid_keys_combo(HID_MOD_LSHIFT, 0);
}

void hid_keys_forget_pairing(void)
{
    ble_store_clear();
    ESP_LOGW(TAG, "all pairings removed");
}

const char *hid_keys_transport(void)
{
    return "BLE";
}

#endif /* CONFIG_AUTOLOCK_HID_BLE */
