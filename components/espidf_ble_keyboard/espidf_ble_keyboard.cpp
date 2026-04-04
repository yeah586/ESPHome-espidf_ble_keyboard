/**
 * BLE HID Keyboard for ESPHome — Fixed Raw Advertising & YAML Passkey logic.
 */
#include "espidf_ble_keyboard.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_gatt_defs.h"
#include "esp_bt_defs.h"
#include "nvs.h"
#include "esp_random.h"
#include <cstring>
#include <cstdio>
#include <vector>

namespace esphome {
namespace espidf_ble_keyboard {

static const char *TAG = "espidf_ble_keyboard";
static EspidfBleKeyboard *s_instance = nullptr;
#define GATTS_APP_ID 0x55

// Forward declarations
static esp_err_t send_keyboard_input_report(uint16_t conn_id, const uint8_t *report, uint16_t len);

// ── HID Report Descriptor ────────────────────────────────────────────────────
// Report ID 1: Standard keyboard (8 bytes)
// Report ID 2: Consumer control — media keys (2 bytes)
// Report ID 3: System control — power/sleep (1 byte)
// Report ID 4: Mouse — buttons + X/Y + scroll (4 bytes)
static const uint8_t hid_report_map[] = {
    // ---- Keyboard (Report ID 1) ----
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
    0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0xC0,
    // ---- Consumer Control (Report ID 2) — media keys ----
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
    0x19, 0x00,        //   Usage Minimum (0)
    0x2A, 0xFF, 0x03,  //   Usage Maximum (1023)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data, Array)
    0xC0,              // End Collection
    // ---- System Control (Report ID 3) — power/sleep ----
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x80,        // Usage (System Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x03,        //   Report ID (3)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0xFF,        //   Usage Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data, Array)
    0xC0,              // End Collection
    // ---- Mouse (Report ID 4) — buttons + X/Y movement + scroll wheel ----
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x04,        //   Report ID (4)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (Button 1 — Left)
    0x29, 0x03,        //     Usage Maximum (Button 3 — Middle)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0x75, 0x05,        //     Report Size (5) — padding
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x01,        //     Input (Constant) — padding to byte boundary
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x06,        //     Input (Data, Variable, Relative)
    0xC0,              //   End Collection (Physical)
    0xC0               // End Collection (Application)
};

// ── ASCII → USB HID Keycode Lookup Table (US keyboard layout) ───────────────
struct HidKeyMapping {
    uint8_t modifier;  // 0x00 = none, 0x02 = Left Shift
    uint8_t keycode;   // USB HID keycode, 0x00 = unmapped
};

static const HidKeyMapping HID_ASCII_MAP[128] = {
    // 0x00 - 0x07: control characters (unmapped)
    {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
    {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
    // 0x08 - 0x0F
    {0x00, 0x00}, // 0x08 BS
    {0x00, 0x2B}, // 0x09 TAB
    {0x00, 0x28}, // 0x0A LF (\n) → Enter
    {0x00, 0x00}, // 0x0B VT
    {0x00, 0x00}, // 0x0C FF
    {0x00, 0x28}, // 0x0D CR (\r) → Enter
    {0x00, 0x00}, // 0x0E
    {0x00, 0x00}, // 0x0F
    // 0x10 - 0x1F: control characters (unmapped)
    {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
    {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
    {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
    {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
    // 0x20 - 0x2F: space, punctuation, digits
    {0x00, 0x2C}, // 0x20 ' ' Space
    {0x02, 0x1E}, // 0x21 '!' Shift+1
    {0x02, 0x34}, // 0x22 '"' Shift+'
    {0x02, 0x20}, // 0x23 '#' Shift+3
    {0x02, 0x21}, // 0x24 '$' Shift+4
    {0x02, 0x22}, // 0x25 '%' Shift+5
    {0x02, 0x24}, // 0x26 '&' Shift+7
    {0x00, 0x34}, // 0x27 '\''
    {0x02, 0x26}, // 0x28 '(' Shift+9
    {0x02, 0x27}, // 0x29 ')' Shift+0
    {0x02, 0x25}, // 0x2A '*' Shift+8
    {0x02, 0x2E}, // 0x2B '+' Shift+=
    {0x00, 0x36}, // 0x2C ','
    {0x00, 0x2D}, // 0x2D '-'
    {0x00, 0x37}, // 0x2E '.'
    {0x00, 0x38}, // 0x2F '/'
    // 0x30 - 0x39: digits 0-9
    {0x00, 0x27}, // 0x30 '0'
    {0x00, 0x1E}, // 0x31 '1'
    {0x00, 0x1F}, // 0x32 '2'
    {0x00, 0x20}, // 0x33 '3'
    {0x00, 0x21}, // 0x34 '4'
    {0x00, 0x22}, // 0x35 '5'
    {0x00, 0x23}, // 0x36 '6'
    {0x00, 0x24}, // 0x37 '7'
    {0x00, 0x25}, // 0x38 '8'
    {0x00, 0x26}, // 0x39 '9'
    // 0x3A - 0x3F: punctuation
    {0x02, 0x33}, // 0x3A ':' Shift+;
    {0x00, 0x33}, // 0x3B ';'
    {0x02, 0x36}, // 0x3C '<' Shift+,
    {0x00, 0x2E}, // 0x3D '='
    {0x02, 0x37}, // 0x3E '>' Shift+.
    {0x02, 0x38}, // 0x3F '?' Shift+/
    // 0x40: @
    {0x02, 0x1F}, // 0x40 '@' Shift+2
    // 0x41 - 0x5A: uppercase A-Z
    {0x02, 0x04}, {0x02, 0x05}, {0x02, 0x06}, {0x02, 0x07},
    {0x02, 0x08}, {0x02, 0x09}, {0x02, 0x0A}, {0x02, 0x0B},
    {0x02, 0x0C}, {0x02, 0x0D}, {0x02, 0x0E}, {0x02, 0x0F},
    {0x02, 0x10}, {0x02, 0x11}, {0x02, 0x12}, {0x02, 0x13},
    {0x02, 0x14}, {0x02, 0x15}, {0x02, 0x16}, {0x02, 0x17},
    {0x02, 0x18}, {0x02, 0x19}, {0x02, 0x1A}, {0x02, 0x1B},
    {0x02, 0x1C}, {0x02, 0x1D},
    // 0x5B - 0x60: punctuation
    {0x00, 0x2F}, // 0x5B '['
    {0x00, 0x31}, // 0x5C '\'
    {0x00, 0x30}, // 0x5D ']'
    {0x02, 0x23}, // 0x5E '^' Shift+6
    {0x02, 0x2D}, // 0x5F '_' Shift+-
    {0x00, 0x35}, // 0x60 '`'
    // 0x61 - 0x7A: lowercase a-z
    {0x00, 0x04}, {0x00, 0x05}, {0x00, 0x06}, {0x00, 0x07},
    {0x00, 0x08}, {0x00, 0x09}, {0x00, 0x0A}, {0x00, 0x0B},
    {0x00, 0x0C}, {0x00, 0x0D}, {0x00, 0x0E}, {0x00, 0x0F},
    {0x00, 0x10}, {0x00, 0x11}, {0x00, 0x12}, {0x00, 0x13},
    {0x00, 0x14}, {0x00, 0x15}, {0x00, 0x16}, {0x00, 0x17},
    {0x00, 0x18}, {0x00, 0x19}, {0x00, 0x1A}, {0x00, 0x1B},
    {0x00, 0x1C}, {0x00, 0x1D},
    // 0x7B - 0x7F: punctuation + DEL
    {0x02, 0x2F}, // 0x7B '{' Shift+[
    {0x02, 0x31}, // 0x7C '|' Shift+'\'
    {0x02, 0x30}, // 0x7D '}' Shift+]
    {0x02, 0x35}, // 0x7E '~' Shift+`
    {0x00, 0x00}, // 0x7F DEL (unmapped)
};

// ── Advertising Data ─────────────────────────────────────────────────────────
static uint8_t raw_adv_data[] = {
    0x02, 0x01, 0x06,           // Flags: LE General Discoverable + BR/EDR not supported
    0x03, 0x03, 0x12, 0x18,     // Complete List of 16-bit UUIDs: HID (0x1812)
    0x03, 0x19, 0xC1, 0x03      // Appearance: HID Keyboard (0x03C1)
};


static esp_ble_adv_params_t adv_params = {
    .adv_int_min       = 0x20,
    .adv_int_max       = 0x40,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr         = {0},
    .peer_addr_type    = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static bool s_adv_data_set = false;
static bool s_scan_rsp_data_set = false;
static bool s_use_static_passkey = false;
static bool s_require_mitm = false;

// Multi-host: when true, next advertising cycle uses directed advertising to target host
static bool s_directed_adv_pending = false;
  static std::atomic<bool> s_directed_adv_active{false};
  static std::atomic<uint32_t> s_directed_adv_start_ms{0};
  static esp_bd_addr_t s_directed_addr = {};
static esp_ble_addr_type_t s_directed_addr_type = BLE_ADDR_TYPE_PUBLIC;


static void maybe_reset_bonds_after_security_config_change() {
    if (s_instance == nullptr) {
        return;
    }

    const bool has_passkey = s_instance->has_passkey();
    const uint8_t current_has_passkey = has_passkey ? 1 : 0;
    const uint32_t current_passkey = has_passkey ? s_instance->passkey() : 0;
    const uint8_t current_sc_mode = s_instance->passkey_secure_connections() ? 1 : 0;

    nvs_handle_t handle;
    esp_err_t open_ret = nvs_open("espidf_ble_kb", NVS_READWRITE, &handle);
    if (open_ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS: Failed to open espidf_ble_kb namespace (%d)", open_ret);
        return;
    }

    uint8_t stored_has_passkey = 0;
    uint32_t stored_passkey = 0;
    uint8_t stored_sc_mode = 0;
    bool has_stored_security_cfg = false;

    esp_err_t has_pk_ret = nvs_get_u8(handle, "has_pk", &stored_has_passkey);
    esp_err_t passkey_ret = nvs_get_u32(handle, "passkey", &stored_passkey);
    esp_err_t sc_ret = nvs_get_u8(handle, "pk_sc", &stored_sc_mode);

    if (has_pk_ret == ESP_OK || passkey_ret == ESP_OK || sc_ret == ESP_OK) {
        has_stored_security_cfg = true;
    }

    bool security_cfg_changed = false;
    if (has_stored_security_cfg) {
        security_cfg_changed = (stored_has_passkey != current_has_passkey) ||
                               (stored_sc_mode != current_sc_mode) ||
                               ((current_has_passkey == 1) && (stored_passkey != current_passkey));
    }

    if (security_cfg_changed) {
        ESP_LOGW(TAG, "Security config changed (passkey/mode). Clearing stored BLE bonds.");
        int dev_num = esp_ble_get_bond_device_num();
        if (dev_num > 0) {
            std::vector<esp_ble_bond_dev_t> bonded(static_cast<size_t>(dev_num));
            int query_num = dev_num;
            esp_err_t list_ret = esp_ble_get_bond_device_list(&query_num, bonded.data());
            if (list_ret == ESP_OK) {
                for (int i = 0; i < query_num; i++) {
                    esp_err_t rm_ret = esp_ble_remove_bond_device(bonded[static_cast<size_t>(i)].bd_addr);
                    if (rm_ret != ESP_OK) {
                        ESP_LOGW(TAG, "Failed to remove bond #%d (%d)", i, rm_ret);
                    }
                }
            } else {
                ESP_LOGW(TAG, "Failed to read bonded device list (%d)", list_ret);
            }
        }
    }

    nvs_set_u8(handle, "has_pk", current_has_passkey);
    nvs_set_u32(handle, "passkey", current_passkey);
    nvs_set_u8(handle, "pk_sc", current_sc_mode);
    nvs_commit(handle);
    nvs_close(handle);
}

static void request_host_friendly_conn_params(const esp_bd_addr_t bda) {
    esp_ble_conn_update_params_t conn_params = {};
    memcpy(conn_params.bda, bda, sizeof(esp_bd_addr_t));
    conn_params.min_int = 0x06;  // 7.5ms — BLE spec minimum, ideal for HID
    conn_params.max_int = 0x0C;  // 15ms
    conn_params.latency = 0;
    conn_params.timeout = 400;

    esp_err_t ret = esp_ble_gap_update_conn_params(&conn_params);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "GAP: Conn param update request failed (%d)", ret);
    }
}

static void apply_security_params(bool use_static_passkey) {
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    // Resolve effective passkey: per-slot overrides global
    bool effective_has_passkey = false;
    uint32_t effective_passkey = 0;
    bool effective_sc = false;
    if (s_instance) {
        s_instance->get_active_slot_passkey(effective_has_passkey, effective_passkey, effective_sc);
    }

    if (use_static_passkey && effective_has_passkey) {
        bool use_sc = effective_sc;
        if (use_sc) {
#if defined(ESP_LE_AUTH_REQ_SC_MITM_BOND)
            auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
            ESP_LOGI(TAG, "Pairing mode: Static passkey (secure-connections MITM bond)");
#elif defined(ESP_LE_AUTH_REQ_MITM_BOND)
            auth_req = ESP_LE_AUTH_REQ_MITM_BOND;
            ESP_LOGW(TAG, "Pairing mode secure_connections requested, but SC MITM constant unavailable; using legacy MITM bond");
#elif defined(ESP_LE_AUTH_REQ_MITM)
            auth_req = static_cast<esp_ble_auth_req_t>(ESP_LE_AUTH_BOND | ESP_LE_AUTH_REQ_MITM);
            ESP_LOGW(TAG, "Pairing mode secure_connections requested, using MITM fallback");
#else
            auth_req = ESP_LE_AUTH_BOND;
            ESP_LOGW(TAG, "Pairing mode secure_connections requested, but MITM unavailable; using bond-only mode");
#endif
        } else {
#if defined(ESP_LE_AUTH_REQ_MITM_BOND)
            auth_req = ESP_LE_AUTH_REQ_MITM_BOND;
#elif defined(ESP_LE_AUTH_REQ_MITM)
            auth_req = static_cast<esp_ble_auth_req_t>(ESP_LE_AUTH_BOND | ESP_LE_AUTH_REQ_MITM);
#else
            auth_req = ESP_LE_AUTH_BOND;
#endif
            ESP_LOGI(TAG, "Pairing mode: Static passkey (legacy MITM bond)");
        }
        iocap = ESP_IO_CAP_OUT;
        uint32_t passkey = effective_passkey;
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(passkey));
        s_use_static_passkey = true;
        s_require_mitm = true;
        ESP_LOGI(TAG, "Setting passkey: %06lu (slot %u)", (unsigned long) passkey,
                 s_instance ? s_instance->active_host_slot() : 0);
    } else {
#if defined(ESP_LE_AUTH_REQ_SC_BOND)
        auth_req = ESP_LE_AUTH_REQ_SC_BOND;
#else
        auth_req = ESP_LE_AUTH_BOND;
#endif
        s_use_static_passkey = false;
        s_require_mitm = false;
        ESP_LOGI(TAG, "Pairing mode: Just Works / host-selected secure bonding");
    }

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
}

static void do_start_advertising() {
    // Set per-slot random address so each slot appears as a different BLE device.
    // This prevents hosts bonded to other slots from auto-reconnecting.
    if (s_instance) {
        uint8_t slot = s_instance->active_host_slot();
        const uint8_t *laddr = s_instance->get_slot_addr(slot);
        esp_ble_gap_set_rand_addr(const_cast<uint8_t *>(laddr));
        adv_params.own_addr_type = BLE_ADDR_TYPE_RANDOM;
        ESP_LOGD(TAG, "ADV: Using slot %u addr %02X:%02X:%02X:%02X:%02X:%02X", slot,
                 laddr[0], laddr[1], laddr[2], laddr[3], laddr[4], laddr[5]);
    }

    // If directed advertising is requested, target the specific bonded host
    if (s_directed_adv_pending) {
        s_directed_adv_pending = false;
        s_directed_adv_active = true;
        s_directed_adv_start_ms = millis();
        ESP_LOGI(TAG, "ADV: Directed advertising to %02X:%02X:%02X:%02X:%02X:%02X",
                 s_directed_addr[0], s_directed_addr[1], s_directed_addr[2],
                 s_directed_addr[3], s_directed_addr[4], s_directed_addr[5]);
        esp_ble_adv_params_t dir_params = adv_params;
        dir_params.adv_type = ADV_TYPE_DIRECT_IND_HIGH;
        memcpy(dir_params.peer_addr, s_directed_addr, sizeof(esp_bd_addr_t));
        dir_params.peer_addr_type = s_directed_addr_type;
        // Set adv data then start (directed low-duty still needs adv data on some stacks)
        s_adv_data_set = false;
        s_scan_rsp_data_set = false;
        esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
        std::string dev_name = (s_instance != nullptr) ? s_instance->device_name() : "ESP32 BLE KB";
        std::vector<uint8_t> scan_rsp;
        scan_rsp.push_back(static_cast<uint8_t>(dev_name.length() + 1));
        scan_rsp.push_back(0x09);
        for (char c : dev_name) scan_rsp.push_back(static_cast<uint8_t>(c));
        esp_ble_gap_config_scan_rsp_data_raw(scan_rsp.data(), static_cast<uint16_t>(scan_rsp.size()));
        // Override adv_params for this cycle — the GAP completion handler will use dir_params
        adv_params.adv_type = ADV_TYPE_DIRECT_IND_HIGH;
        memcpy(adv_params.peer_addr, s_directed_addr, sizeof(esp_bd_addr_t));
        adv_params.peer_addr_type = s_directed_addr_type;
        return;
    }

    // Normal undirected advertising (pairing mode / default)
    adv_params.adv_type = ADV_TYPE_IND;
    memset(adv_params.peer_addr, 0, sizeof(esp_bd_addr_t));
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    s_adv_data_set = false;
    s_scan_rsp_data_set = false;
    esp_err_t adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
    std::string dev_name = (s_instance != nullptr) ? s_instance->device_name() : "ESP32 BLE KB";
    std::vector<uint8_t> scan_rsp;
    scan_rsp.push_back(static_cast<uint8_t>(dev_name.length() + 1));
    scan_rsp.push_back(0x09);  // Complete Local Name AD type
    for (char c : dev_name) scan_rsp.push_back(static_cast<uint8_t>(c));
    esp_err_t scan_ret = esp_ble_gap_config_scan_rsp_data_raw(scan_rsp.data(), static_cast<uint16_t>(scan_rsp.size()));

    if (adv_ret != ESP_OK) {
        ESP_LOGE(TAG, "GAP: Failed to config adv data (%d)", adv_ret);
        s_adv_data_set = true;
    }
    if (scan_ret != ESP_OK) {
        ESP_LOGE(TAG, "GAP: Failed to config scan rsp data (%d)", scan_ret);
        s_scan_rsp_data_set = true;
    }
    if (s_adv_data_set && s_scan_rsp_data_set) {
        esp_ble_gap_start_advertising(&adv_params);
    }
}

// ── GAP Event Handler ────────────────────────────────────────────────────────
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            s_adv_data_set = true;
            if (s_scan_rsp_data_set) esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
            s_scan_rsp_data_set = true;
            if (s_adv_data_set) esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "GAP: Advertising started");
            } else {
                ESP_LOGE(TAG, "GAP: Advertising start failed (%d)", param->adv_start_cmpl.status);
            }
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT:
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT:
            if (s_instance && s_use_static_passkey) {
                bool pk_has; uint32_t pk_val; bool pk_sc;
                s_instance->get_active_slot_passkey(pk_has, pk_val, pk_sc);
                esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, pk_val);
            } else {
                esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, false, 0);
            }
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
            ESP_LOGD(TAG, "GAP: Passkey %06lu", (unsigned long) param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_NC_REQ_EVT:
            esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            if (param->ble_security.auth_cmpl.success) {
                ESP_LOGI(TAG, "GAP: Pairing Successful");
                if (s_instance) {
                    s_instance->queue_paired_state(true);
                    // Check if this host is already in a known slot
                    bool already_known = false;
                    for (uint8_t i = 0; i < MAX_HOST_SLOTS; i++) {
                        auto &hs = s_instance->get_host_slot(i);
                        if (hs.occupied && memcmp(hs.addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t)) == 0) {
                            already_known = true;
                            ESP_LOGI(TAG, "Reconnected host already in slot %u", i);
                            break;
                        }
                    }
                    if (!already_known) {
                        // New host — assign to the active slot
                        s_instance->assign_host_slot_(
                            s_instance->active_host_slot(),
                            param->ble_security.auth_cmpl.bd_addr,
                            (esp_ble_addr_type_t) param->ble_security.auth_cmpl.addr_type);
                        s_instance->save_host_slots_();
                    }
                }
            } else {
                uint8_t fail_reason = param->ble_security.auth_cmpl.fail_reason;
                ESP_LOGE(TAG, "GAP: Pairing Failed (0x%x)", fail_reason);
                bool fb_has, fb_sc; uint32_t fb_pk;
                if (s_instance) s_instance->get_active_slot_passkey(fb_has, fb_pk, fb_sc);
                if (s_instance &&
                    fb_has &&
                    s_use_static_passkey &&
                    !fb_sc &&
                    fail_reason == 0x51) {
                    ESP_LOGW(TAG, "GAP: Static passkey rejected by peer (0x51), falling back to Just Works mode");
                    apply_security_params(false);
                    esp_ble_remove_bond_device(param->ble_security.auth_cmpl.bd_addr);
                }
                // Advertising restart is handled in DISCONNECT_EVT to avoid duplicate restarts.
            }
            break;
        case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
            // Only update paired state if we're actually connected.
            // During 0x51 passkey fallback, bond removal happens while disconnected
            // and should not briefly flash the paired sensor ON.
            if (s_instance && s_instance->is_connected()) {
                s_instance->queue_paired_state(esp_ble_get_bond_device_num() > 0);
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGD(TAG, "GAP: Conn params updated (status=%d int=%u latency=%u timeout=%u)",
                     param->update_conn_params.status,
                     param->update_conn_params.conn_int,
                     param->update_conn_params.latency,
                     param->update_conn_params.timeout);
            break;
        case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
            if (s_instance) {
                s_instance->rssi_pending_ = false;
                if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                    s_instance->update_rssi(param->read_rssi_cmpl.rssi);
                }
            }
            break;
        default:
            break;
    }
}

// ── GATT Attribute Tables (one per service — ESP-IDF requires separate tables) ─

// Encrypted permission shorthands — iOS requires these on HID characteristics
#define PERM_R          ESP_GATT_PERM_READ
#define PERM_RW         (ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE)
#define PERM_R_ENC      ESP_GATT_PERM_READ_ENCRYPTED
#define PERM_W_ENC      ESP_GATT_PERM_WRITE_ENCRYPTED
#define PERM_RW_ENC     (ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED)

static const uint16_t UUID_PRI_SERVICE        = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t UUID_CHAR_DECLARE       = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t UUID_CHAR_CLIENT_CONFIG = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint16_t UUID_RPT_REF_DESCR      = ESP_GATT_UUID_RPT_REF_DESCR;
static const uint16_t UUID_DIS_SVC            = 0x180A;
static const uint16_t UUID_PNP_ID             = 0x2A50;
static const uint16_t UUID_MFR_NAME           = 0x2A29;
static const uint16_t UUID_BAS_SVC            = 0x180F;
static const uint16_t UUID_BATTERY_LEVEL      = 0x2A19;
static const uint16_t UUID_HID_SVC            = ESP_GATT_UUID_HID_SVC;
static const uint16_t UUID_HID_INFORMATION    = ESP_GATT_UUID_HID_INFORMATION;
static const uint16_t UUID_HID_REPORT_MAP     = ESP_GATT_UUID_HID_REPORT_MAP;
static const uint16_t UUID_HID_CONTROL_POINT  = ESP_GATT_UUID_HID_CONTROL_POINT;
static const uint16_t UUID_HID_PROTO_MODE     = ESP_GATT_UUID_HID_PROTO_MODE;
static const uint16_t UUID_HID_REPORT         = ESP_GATT_UUID_HID_REPORT;
static const uint16_t UUID_HID_BOOT_KB_INPUT  = 0x2A22;
static const uint16_t UUID_HID_BOOT_KB_OUTPUT = 0x2A32;

static const uint8_t PROP_READ        = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t PROP_WRITE_NR    = ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t PROP_RW_NR       = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t PROP_READ_WRITE  = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t PROP_READ_NOTIFY = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

// ── DIS (Device Information Service) ─────────────────────────────────────────
static uint8_t  pnp_id_val[7]     = {0x01, 0xE5, 0x02, 0xB2, 0xA1, 0x00, 0x01};
static const char mfr_name_val[]  = "Espressif";

enum { DIS_IDX_SVC, DIS_IDX_PNP_CHAR, DIS_IDX_PNP_VAL, DIS_IDX_MFR_CHAR, DIS_IDX_MFR_VAL, DIS_IDX_NB };
static uint16_t dis_handle_table[DIS_IDX_NB];
static const esp_gatts_attr_db_t dis_attr_db[DIS_IDX_NB] = {
    [DIS_IDX_SVC]      = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_PRI_SERVICE, PERM_R, 2, 2, (uint8_t *)&UUID_DIS_SVC}},
    [DIS_IDX_PNP_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ}},
    [DIS_IDX_PNP_VAL]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_PNP_ID, PERM_R, sizeof(pnp_id_val), sizeof(pnp_id_val), pnp_id_val}},
    [DIS_IDX_MFR_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ}},
    [DIS_IDX_MFR_VAL]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_MFR_NAME, PERM_R, sizeof(mfr_name_val) - 1, sizeof(mfr_name_val) - 1, (uint8_t *)mfr_name_val}},
};

// ── BAS (Battery Service) ────────────────────────────────────────────────────
static uint8_t  battery_level_val = 100;
static uint16_t battery_ccc_val   = 0;

enum { BAS_IDX_SVC, BAS_IDX_BAT_CHAR, BAS_IDX_BAT_VAL, BAS_IDX_BAT_CCC, BAS_IDX_NB };
static uint16_t bas_handle_table[BAS_IDX_NB];
static const esp_gatts_attr_db_t bas_attr_db[BAS_IDX_NB] = {
    [BAS_IDX_SVC]      = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_PRI_SERVICE, PERM_R, 2, 2, (uint8_t *)&UUID_BAS_SVC}},
    [BAS_IDX_BAT_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [BAS_IDX_BAT_VAL]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_BATTERY_LEVEL, PERM_R, 1, 1, &battery_level_val}},
    [BAS_IDX_BAT_CCC]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW, 2, 2, (uint8_t *)&battery_ccc_val}},
};

// ── HID Service ──────────────────────────────────────────────────────────────
static uint8_t  hid_info_val[4]       = {0x11, 0x01, 0x00, 0x03};
static uint8_t  hid_ctrl_val          = 0;
static uint8_t  proto_mode_val        = 0x01;
static uint8_t  boot_kb_in_val[8]     = {0};
static uint16_t boot_kb_in_ccc_val    = 0;
static uint8_t  boot_kb_out_val[1]    = {0};
static uint8_t  report_val[8]         = {0};
static uint16_t report_ccc_val        = 0;
static uint8_t  report_ref_val[2]     = {0x01, 0x01};
static uint8_t  report_out_val[1]     = {0};
static uint8_t  report_out_ref_val[2] = {0x01, 0x02};
static uint8_t  consumer_val[2]       = {0};
static uint16_t consumer_ccc_val      = 0;
static uint8_t  consumer_ref_val[2]   = {0x02, 0x01};
static uint8_t  system_val            = 0;
static uint16_t system_ccc_val        = 0;
static uint8_t  system_ref_val[2]     = {0x03, 0x01};
static uint8_t  mouse_val[4]          = {0};  // buttons, X, Y, wheel
static uint16_t mouse_ccc_val         = 0;
static uint8_t  mouse_ref_val[2]      = {0x04, 0x01};

enum {
    IDX_SVC,
    IDX_CHAR_HID_INFO,     IDX_CHAR_HID_INFO_VAL,
    IDX_CHAR_REPORT_MAP,   IDX_CHAR_REPORT_MAP_VAL,
    IDX_CHAR_HID_CTRL,     IDX_CHAR_HID_CTRL_VAL,
    IDX_CHAR_PROTO_MODE,   IDX_CHAR_PROTO_MODE_VAL,
    IDX_CHAR_BOOT_KB_IN,   IDX_CHAR_BOOT_KB_IN_VAL,
    IDX_CHAR_BOOT_KB_IN_CCC,
    IDX_CHAR_BOOT_KB_OUT,  IDX_CHAR_BOOT_KB_OUT_VAL,
    IDX_CHAR_REPORT,       IDX_CHAR_REPORT_VAL,
    IDX_CHAR_REPORT_CCC,
    IDX_CHAR_REPORT_REF,
    IDX_CHAR_REPORT_OUT,   IDX_CHAR_REPORT_OUT_VAL,
    IDX_CHAR_REPORT_OUT_REF,
    IDX_CHAR_CONSUMER,     IDX_CHAR_CONSUMER_VAL,
    IDX_CHAR_CONSUMER_CCC,
    IDX_CHAR_CONSUMER_REF,
    IDX_CHAR_SYSTEM,       IDX_CHAR_SYSTEM_VAL,
    IDX_CHAR_SYSTEM_CCC,
    IDX_CHAR_SYSTEM_REF,
    IDX_CHAR_MOUSE,        IDX_CHAR_MOUSE_VAL,
    IDX_CHAR_MOUSE_CCC,
    IDX_CHAR_MOUSE_REF,
    HID_IDX_NB,
};

static uint16_t hid_handle_table[HID_IDX_NB];
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_hid_report_handle = 0;
static uint16_t s_hid_output_report_handle = 0;
static uint16_t s_boot_kb_input_handle = 0;
static uint16_t s_boot_kb_output_handle = 0;
static uint16_t s_proto_mode_handle = 0;
static uint16_t s_boot_kb_input_ccc_handle = 0;
static uint16_t s_hid_report_ccc_handle = 0;
static uint16_t s_consumer_report_handle = 0;
static uint16_t s_consumer_ccc_handle = 0;
static uint16_t s_system_report_handle = 0;
static uint16_t s_system_ccc_handle = 0;
static uint16_t s_mouse_report_handle = 0;
static uint16_t s_mouse_ccc_handle = 0;

static const esp_gatts_attr_db_t hid_attr_db[HID_IDX_NB] = {
    [IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_PRI_SERVICE, PERM_R, sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&UUID_HID_SVC}},
    [IDX_CHAR_HID_INFO] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ}},
    [IDX_CHAR_HID_INFO_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_INFORMATION, PERM_R_ENC, sizeof(hid_info_val), sizeof(hid_info_val), hid_info_val}},
    [IDX_CHAR_REPORT_MAP] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ}},
    [IDX_CHAR_REPORT_MAP_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT_MAP, PERM_R_ENC, sizeof(hid_report_map), sizeof(hid_report_map), (uint8_t *)hid_report_map}},
    [IDX_CHAR_HID_CTRL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_WRITE_NR}},
    [IDX_CHAR_HID_CTRL_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_CONTROL_POINT, PERM_W_ENC, 1, 1, &hid_ctrl_val}},
    [IDX_CHAR_PROTO_MODE] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_RW_NR}},
    [IDX_CHAR_PROTO_MODE_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_PROTO_MODE, PERM_RW_ENC, 1, 1, &proto_mode_val}},
    // Boot keyboard input report
    [IDX_CHAR_BOOT_KB_IN] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_BOOT_KB_IN_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_BOOT_KB_INPUT, PERM_R_ENC, sizeof(boot_kb_in_val), sizeof(boot_kb_in_val), boot_kb_in_val}},
    [IDX_CHAR_BOOT_KB_IN_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(boot_kb_in_ccc_val), sizeof(boot_kb_in_ccc_val), (uint8_t *)&boot_kb_in_ccc_val}},
    // Boot keyboard output report
    [IDX_CHAR_BOOT_KB_OUT] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_WRITE}},
    [IDX_CHAR_BOOT_KB_OUT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_BOOT_KB_OUTPUT, PERM_RW_ENC, sizeof(boot_kb_out_val), sizeof(boot_kb_out_val), boot_kb_out_val}},
    [IDX_CHAR_REPORT] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_REPORT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_R_ENC, sizeof(report_val), sizeof(report_val), report_val}},
    [IDX_CHAR_REPORT_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(report_ccc_val), sizeof(report_ccc_val), (uint8_t *)&report_ccc_val}},
    [IDX_CHAR_REPORT_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(report_ref_val), sizeof(report_ref_val), report_ref_val}},
    [IDX_CHAR_REPORT_OUT] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_WRITE}},
    [IDX_CHAR_REPORT_OUT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_RW_ENC, sizeof(report_out_val), sizeof(report_out_val), report_out_val}},
    [IDX_CHAR_REPORT_OUT_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(report_out_ref_val), sizeof(report_out_ref_val), report_out_ref_val}},
    // Consumer control report (Report ID 2)
    [IDX_CHAR_CONSUMER] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_CONSUMER_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_R_ENC, sizeof(consumer_val), sizeof(consumer_val), consumer_val}},
    [IDX_CHAR_CONSUMER_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(consumer_ccc_val), sizeof(consumer_ccc_val), (uint8_t *)&consumer_ccc_val}},
    [IDX_CHAR_CONSUMER_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(consumer_ref_val), sizeof(consumer_ref_val), consumer_ref_val}},
    // System control report (Report ID 3)
    [IDX_CHAR_SYSTEM] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_SYSTEM_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_R_ENC, sizeof(system_val), sizeof(system_val), &system_val}},
    [IDX_CHAR_SYSTEM_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(system_ccc_val), sizeof(system_ccc_val), (uint8_t *)&system_ccc_val}},
    [IDX_CHAR_SYSTEM_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(system_ref_val), sizeof(system_ref_val), system_ref_val}},
    // Mouse report (Report ID 4)
    [IDX_CHAR_MOUSE] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_MOUSE_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_R_ENC, sizeof(mouse_val), sizeof(mouse_val), mouse_val}},
    [IDX_CHAR_MOUSE_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(mouse_ccc_val), sizeof(mouse_ccc_val), (uint8_t *)&mouse_ccc_val}},
    [IDX_CHAR_MOUSE_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(mouse_ref_val), sizeof(mouse_ref_val), mouse_ref_val}},
};

// Service instance IDs for create_attr_tab
#define SVC_INST_DIS  0
#define SVC_INST_BAS  1
#define SVC_INST_HID  2

static uint8_t s_services_started = 0;

// ── GATTS Event Handler ──────────────────────────────────────────────────────
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            s_gatts_if = gatts_if;
            esp_ble_gap_set_device_name(s_instance ? s_instance->device_name().c_str() : "ESP32 BLE KB");
            // Create each service as a separate attribute table
            esp_ble_gatts_create_attr_tab(dis_attr_db, gatts_if, DIS_IDX_NB, SVC_INST_DIS);
            esp_ble_gatts_create_attr_tab(bas_attr_db, gatts_if, BAS_IDX_NB, SVC_INST_BAS);
            esp_ble_gatts_create_attr_tab(hid_attr_db, gatts_if, HID_IDX_NB, SVC_INST_HID);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
            if (param->add_attr_tab.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "GATTS: Attr table create failed (svc=%u status=%d)",
                         param->add_attr_tab.svc_inst_id, param->add_attr_tab.status);
                break;
            }
            uint8_t svc_id = param->add_attr_tab.svc_inst_id;
            if (svc_id == SVC_INST_DIS) {
                memcpy(dis_handle_table, param->add_attr_tab.handles, sizeof(dis_handle_table));
                esp_ble_gatts_start_service(dis_handle_table[DIS_IDX_SVC]);
            } else if (svc_id == SVC_INST_BAS) {
                memcpy(bas_handle_table, param->add_attr_tab.handles, sizeof(bas_handle_table));
                esp_ble_gatts_start_service(bas_handle_table[BAS_IDX_SVC]);
            } else if (svc_id == SVC_INST_HID) {
                memcpy(hid_handle_table, param->add_attr_tab.handles, sizeof(hid_handle_table));
                s_proto_mode_handle = hid_handle_table[IDX_CHAR_PROTO_MODE_VAL];
                s_boot_kb_input_handle = hid_handle_table[IDX_CHAR_BOOT_KB_IN_VAL];
                s_boot_kb_input_ccc_handle = hid_handle_table[IDX_CHAR_BOOT_KB_IN_CCC];
                s_boot_kb_output_handle = hid_handle_table[IDX_CHAR_BOOT_KB_OUT_VAL];
                s_hid_report_handle = hid_handle_table[IDX_CHAR_REPORT_VAL];
                s_hid_report_ccc_handle = hid_handle_table[IDX_CHAR_REPORT_CCC];
                s_hid_output_report_handle = hid_handle_table[IDX_CHAR_REPORT_OUT_VAL];
                s_consumer_report_handle = hid_handle_table[IDX_CHAR_CONSUMER_VAL];
                s_consumer_ccc_handle = hid_handle_table[IDX_CHAR_CONSUMER_CCC];
                s_system_report_handle = hid_handle_table[IDX_CHAR_SYSTEM_VAL];
                s_system_ccc_handle = hid_handle_table[IDX_CHAR_SYSTEM_CCC];
                s_mouse_report_handle = hid_handle_table[IDX_CHAR_MOUSE_VAL];
                s_mouse_ccc_handle = hid_handle_table[IDX_CHAR_MOUSE_CCC];
                esp_ble_gatts_start_service(hid_handle_table[IDX_SVC]);
            }
            break;
        }
        case ESP_GATTS_START_EVT:
            s_services_started++;
            ESP_LOGD(TAG, "GATTS: Service started (%u/3)", s_services_started);
            if (s_services_started < 3) break;
            ESP_LOGI(TAG, "GATTS: All services started (DIS + BAS + HID)");
            do_start_advertising();
            break;
        case ESP_GATTS_CONNECT_EVT: {
            ESP_LOGI(TAG, "GATTS: Connected");
            if (s_instance) {
                s_instance->set_connected(true, param->connect.conn_id);
                memcpy(s_instance->peer_addr_, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            }
            proto_mode_val = 0x01;
            report_ccc_val = 0;
            boot_kb_in_ccc_val = 0;
            consumer_ccc_val = 0;
            system_ccc_val = 0;
            mouse_ccc_val = 0;
            battery_ccc_val = 0;
            request_host_friendly_conn_params(param->connect.remote_bda);
            // Trigger encryption with security level matching configured pairing mode
            esp_ble_sec_act_t sec_act = s_require_mitm ? ESP_BLE_SEC_ENCRYPT_MITM : ESP_BLE_SEC_ENCRYPT_NO_MITM;
            esp_ble_set_encryption(param->connect.remote_bda, sec_act);
            break;
        }
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "GATTS: Disconnected");
            ESP_LOGD(TAG, "GATTS: Disconnect reason 0x%02X", param->disconnect.reason);
            if (s_instance) {
                s_instance->set_connected(false, 0);
                // Host-side unpair often appears only as disconnect.
                s_instance->queue_paired_state(false);
                if (s_instance->rssi_sensor_ != nullptr) {
                    s_instance->rssi_sensor_->publish_state(NAN);
                    s_instance->rssi_pending_ = false;
                }
            }
            proto_mode_val = 0x01;
            report_ccc_val = 0;
            boot_kb_in_ccc_val = 0;
            consumer_ccc_val = 0;
            system_ccc_val = 0;
            mouse_ccc_val = 0;
            battery_ccc_val = 0;
            do_start_advertising();
            break;
        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == s_proto_mode_handle && param->write.len > 0) {
                proto_mode_val = param->write.value[0];
                ESP_LOGD(TAG, "GATTS: Protocol mode set to 0x%02X", proto_mode_val);
            }
            if (param->write.handle == s_hid_report_ccc_handle && param->write.len >= 2) {
                report_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                 (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGD(TAG, "GATTS: Report input CCC=0x%04X", report_ccc_val);
            }
            if (param->write.handle == s_boot_kb_input_ccc_handle && param->write.len >= 2) {
                boot_kb_in_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                     (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGD(TAG, "GATTS: Boot KB input CCC=0x%04X", boot_kb_in_ccc_val);
            }
            if (param->write.handle == s_consumer_ccc_handle && param->write.len >= 2) {
                consumer_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                   (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGI(TAG, "GATTS: Consumer CCC=0x%04X (media keys)", consumer_ccc_val);
            }
            if (param->write.handle == s_system_ccc_handle && param->write.len >= 2) {
                system_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                 (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGI(TAG, "GATTS: System CCC=0x%04X (power/sleep)", system_ccc_val);
            }
            if (param->write.handle == s_mouse_ccc_handle && param->write.len >= 2) {
                mouse_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGI(TAG, "GATTS: Mouse CCC=0x%04X", mouse_ccc_val);
            }
            if ((param->write.handle == s_hid_output_report_handle || param->write.handle == s_boot_kb_output_handle) &&
                param->write.len > 0) {
                ESP_LOGD(TAG, "GATTS: Keyboard LED report 0x%02X", param->write.value[0]);
            }
            break;
        default:
            break;
    }
}

// ── Multi-Host Slot Management ──────────────────────────────────────────────

void EspidfBleKeyboard::generate_slot_addrs_() {
    // Generate random static BLE addresses for each slot and persist to NVS.
    // Random static addresses have the two MSBs set to 11.
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    for (uint8_t i = 0; i < MAX_HOST_SLOTS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "slot%u_laddr", i);
        size_t len = sizeof(esp_bd_addr_t);
        if (nvs_get_blob(handle, key, slot_addrs_[i], &len) == ESP_OK) {
            ESP_LOGI(TAG, "Loaded slot %u local addr: %02X:%02X:%02X:%02X:%02X:%02X", i,
                     slot_addrs_[i][0], slot_addrs_[i][1], slot_addrs_[i][2],
                     slot_addrs_[i][3], slot_addrs_[i][4], slot_addrs_[i][5]);
        } else {
            // Generate new random static address
            esp_fill_random(slot_addrs_[i], 6);
            slot_addrs_[i][0] |= 0xC0;  // MSBs = 11 → random static
            nvs_set_blob(handle, key, slot_addrs_[i], sizeof(esp_bd_addr_t));
            ESP_LOGI(TAG, "Generated slot %u local addr: %02X:%02X:%02X:%02X:%02X:%02X", i,
                     slot_addrs_[i][0], slot_addrs_[i][1], slot_addrs_[i][2],
                     slot_addrs_[i][3], slot_addrs_[i][4], slot_addrs_[i][5]);
        }
    }
    nvs_commit(handle);
    nvs_close(handle);
}

void EspidfBleKeyboard::load_host_slots_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;

    uint8_t slot_count = 0;
    if (nvs_get_u8(handle, "host_cnt", &slot_count) == ESP_OK) {
        for (uint8_t i = 0; i < slot_count && i < MAX_HOST_SLOTS; i++) {
            char key[16];
            snprintf(key, sizeof(key), "host%u_addr", i);
            size_t len = sizeof(esp_bd_addr_t);
            if (nvs_get_blob(handle, key, hosts_[i].addr, &len) == ESP_OK) {
                hosts_[i].occupied = true;
                snprintf(key, sizeof(key), "host%u_type", i);
                uint8_t addr_type = 0;
                if (nvs_get_u8(handle, key, &addr_type) == ESP_OK) {
                    hosts_[i].addr_type = (esp_ble_addr_type_t) addr_type;
                }
                ESP_LOGI(TAG, "Loaded host slot %u: %02X:%02X:%02X:%02X:%02X:%02X", i,
                         hosts_[i].addr[0], hosts_[i].addr[1], hosts_[i].addr[2],
                         hosts_[i].addr[3], hosts_[i].addr[4], hosts_[i].addr[5]);
            }
        }
    }

    uint8_t active = 0;
    if (nvs_get_u8(handle, "host_act", &active) == ESP_OK && active < MAX_HOST_SLOTS) {
        active_slot_ = active;
    }

    nvs_close(handle);
}

void EspidfBleKeyboard::save_host_slots_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_HOST_SLOTS; i++) {
        char key[12];
        if (hosts_[i].occupied) {
            snprintf(key, sizeof(key), "host%u_addr", i);
            nvs_set_blob(handle, key, hosts_[i].addr, sizeof(esp_bd_addr_t));
            snprintf(key, sizeof(key), "host%u_type", i);
            nvs_set_u8(handle, key, (uint8_t) hosts_[i].addr_type);
            count = i + 1;
        } else {
            // Erase stale entries
            snprintf(key, sizeof(key), "host%u_addr", i);
            nvs_erase_key(handle, key);
            snprintf(key, sizeof(key), "host%u_type", i);
            nvs_erase_key(handle, key);
        }
    }
    nvs_set_u8(handle, "host_cnt", count);
    nvs_set_u8(handle, "host_act", active_slot_);
    nvs_commit(handle);
    nvs_close(handle);
}

// ── User-editable macros (NVS-persisted) ──────────────────────────

void EspidfBleKeyboard::load_macros_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;

    uint8_t count = 0;
    if (nvs_get_u8(handle, "macro_cnt", &count) == ESP_OK) {
        for (uint8_t i = 0; i < count && i < MAX_MACROS; i++) {
            char key[16];
            char buf[256];
            size_t len;
            std::string name, action;

            snprintf(key, sizeof(key), "macro%u_name", i);
            len = sizeof(buf);
            if (nvs_get_str(handle, key, buf, &len) == ESP_OK)
                name = buf;

            snprintf(key, sizeof(key), "macro%u_act", i);
            len = sizeof(buf);
            if (nvs_get_str(handle, key, buf, &len) == ESP_OK)
                action = buf;

            if (!name.empty() && !action.empty()) {
                macros_.push_back({name, action});
                ESP_LOGI(TAG, "Loaded macro %u: %s -> %s", i, name.c_str(), action.c_str());
            }
        }
    }
    nvs_close(handle);
}

void EspidfBleKeyboard::save_macros_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    uint8_t count = macros_.size();
    nvs_set_u8(handle, "macro_cnt", count);

    for (uint8_t i = 0; i < MAX_MACROS; i++) {
        char key[16];
        if (i < count) {
            snprintf(key, sizeof(key), "macro%u_name", i);
            nvs_set_str(handle, key, macros_[i].name.c_str());
            snprintf(key, sizeof(key), "macro%u_act", i);
            nvs_set_str(handle, key, macros_[i].action.c_str());
        } else {
            // Erase stale entries
            snprintf(key, sizeof(key), "macro%u_name", i);
            nvs_erase_key(handle, key);
            snprintf(key, sizeof(key), "macro%u_act", i);
            nvs_erase_key(handle, key);
        }
    }
    nvs_commit(handle);
    nvs_close(handle);
}

bool EspidfBleKeyboard::add_macro(const std::string &name, const std::string &action) {
    if (macros_.size() >= MAX_MACROS) return false;
    macros_.push_back({name, action});
    save_macros_();
    ESP_LOGI(TAG, "Added macro: %s -> %s", name.c_str(), action.c_str());
    return true;
}

bool EspidfBleKeyboard::update_macro(uint8_t index, const std::string &name, const std::string &action) {
    if (index >= macros_.size()) return false;
    macros_[index].name = name;
    macros_[index].action = action;
    save_macros_();
    ESP_LOGI(TAG, "Updated macro %u: %s -> %s", index, name.c_str(), action.c_str());
    return true;
}

bool EspidfBleKeyboard::delete_macro(uint8_t index) {
    if (index >= macros_.size()) return false;
    ESP_LOGI(TAG, "Deleted macro %u: %s", index, macros_[index].name.c_str());
    macros_.erase(macros_.begin() + index);
    save_macros_();
    return true;
}

void EspidfBleKeyboard::assign_host_slot_(uint8_t slot, const esp_bd_addr_t addr, esp_ble_addr_type_t addr_type) {
    if (slot >= MAX_HOST_SLOTS) return;
    // Check if this address is already in another slot
    for (uint8_t i = 0; i < MAX_HOST_SLOTS; i++) {
        if (hosts_[i].occupied && memcmp(hosts_[i].addr, addr, sizeof(esp_bd_addr_t)) == 0) {
            if (i == slot) return;  // Already in the right slot
            // Move from old slot to new slot
            hosts_[i].occupied = false;
            break;
        }
    }
    memcpy(hosts_[slot].addr, addr, sizeof(esp_bd_addr_t));
    hosts_[slot].addr_type = addr_type;
    hosts_[slot].occupied = true;
    ESP_LOGI(TAG, "Host slot %u assigned: %02X:%02X:%02X:%02X:%02X:%02X", slot,
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

void EspidfBleKeyboard::switch_host(uint8_t slot) {
    if (slot >= host_slots_) {
        ESP_LOGW(TAG, "Invalid host slot %u (max %u)", slot, host_slots_ - 1);
        return;
    }

    active_slot_ = slot;
    save_host_slots_();
    if (active_host_sensor_ != nullptr)
        active_host_sensor_->publish_state(slot);

    // Re-apply security params for the new slot's passkey config
    bool slot_has_pk; uint32_t slot_pk; bool slot_sc;
    get_active_slot_passkey(slot_has_pk, slot_pk, slot_sc);
    apply_security_params(slot_has_pk);

    ESP_LOGI(TAG, "Switching to host slot %u", slot);

    if (hosts_[slot].occupied) {
        // Directed advertising to the bonded host
        s_directed_adv_pending = true;
        memcpy(s_directed_addr, hosts_[slot].addr, sizeof(esp_bd_addr_t));
        s_directed_addr_type = hosts_[slot].addr_type;
    }
    // else: empty slot — undirected advertising (pairing mode)

    if (is_connected_) {
        // Disconnect current host; DISCONNECT_EVT will trigger advertising
        esp_ble_gatts_close(s_gatts_if, conn_id_);
    } else {
        // Not connected — stop current advertising and restart
        esp_ble_gap_stop_advertising();
        do_start_advertising();
    }
}

void EspidfBleKeyboard::forget_host(uint8_t slot) {
    if (slot >= MAX_HOST_SLOTS || !hosts_[slot].occupied) return;

    ESP_LOGI(TAG, "Forgetting host slot %u", slot);

    // Remove the BLE bond
    esp_ble_remove_bond_device(hosts_[slot].addr);

    // Clear the slot
    hosts_[slot].occupied = false;
    memset(hosts_[slot].addr, 0, sizeof(esp_bd_addr_t));
    hosts_[slot].name.clear();

    save_host_slots_();

    // If this was the active slot and we're connected, disconnect
    if (slot == active_slot_ && is_connected_) {
        esp_ble_gatts_close(s_gatts_if, conn_id_);
    }
}

bool EspidfBleKeyboard::get_active_slot_passkey(bool &has_passkey, uint32_t &passkey, bool &secure_connections) const {
    const auto &cfg = host_slot_configs_[active_slot_];
    if (cfg.has_passkey) {
        has_passkey = true;
        passkey = cfg.passkey;
        secure_connections = cfg.secure_connections;
        return true;
    }
    // Fall back to global config
    has_passkey = has_passkey_;
    passkey = passkey_;
    secure_connections = passkey_secure_connections_;
    return has_passkey_;
}

void EspidfBleKeyboard::update_rssi(int8_t rssi) {
    if (rssi_sensor_ != nullptr) {
        rssi_sensor_->publish_state(static_cast<float>(rssi));
    }
    for (auto &cb : rssi_above_callbacks_) cb(rssi);
    for (auto &cb : rssi_below_callbacks_) cb(rssi);
}

// ── Component Setup ──────────────────────────────────────────────────────────
void EspidfBleKeyboard::setup() {
    s_instance = this;
    type_mutex_ = xSemaphoreCreateMutex();
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    maybe_reset_bonds_after_security_config_change();
    load_host_slots_();
    load_macros_();
    generate_slot_addrs_();
    if (active_host_sensor_ != nullptr)
        active_host_sensor_->publish_state(active_slot_);

    // If active slot has a bonded host, use directed advertising on startup
    if (hosts_[active_slot_].occupied) {
        s_directed_adv_pending = true;
        memcpy(s_directed_addr, hosts_[active_slot_].addr, sizeof(esp_bd_addr_t));
        s_directed_addr_type = hosts_[active_slot_].addr_type;
        ESP_LOGI(TAG, "Startup: will direct-advertise to host slot %u", active_slot_);
    }

    bool startup_has_pk; uint32_t startup_pk; bool startup_sc;
    get_active_slot_passkey(startup_has_pk, startup_pk, startup_sc);
    apply_security_params(startup_has_pk);

    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(GATTS_APP_ID);

    set_connected(false, 0);
    set_paired(false);

#ifdef USE_BLE_KEYBOARD_WEB_CONTROL
    if (web_control_enabled_ && web_server_base_ != nullptr) {
      web_control_ = new BleKeyboardWebControl(web_server_base_, this);
      web_control_->setup();
    }
#endif
}

void EspidfBleKeyboard::loop() {
    if (pending_paired_update_.exchange(false)) {
        set_paired(pending_paired_state_.load());
    }

    if (is_connected_) {
        s_directed_adv_active = false;
    } else if (s_directed_adv_active) {
        if (millis() - s_directed_adv_start_ms > 2000) {
            s_directed_adv_active = false;
            ESP_LOGW(TAG, "ADV: Directed advertising timeout. Falling back to undirected...");
            esp_ble_gap_stop_advertising();
            do_start_advertising();
        }
    }

    // Non-blocking string typing: one keystroke step per loop() call, paced by timer.
    if (!is_connected_ || type_mutex_ == nullptr) return;

    uint32_t now = millis();

    // RSSI polling: read signal strength of connected host on configured interval.
    if (rssi_sensor_ != nullptr && !rssi_pending_) {
        if (now - rssi_last_poll_ms_ >= rssi_update_interval_ms_) {
            rssi_last_poll_ms_ = now;
            rssi_pending_ = true;
            esp_ble_gap_read_rssi(peer_addr_);
        }
    }
    if (now < type_next_ms_) return;

    // Snapshot queue state under mutex (non-blocking try-lock).
    if (xSemaphoreTake(type_mutex_, 0) != pdTRUE) return;
    bool queue_empty = type_queue_.empty();
    bool key_up = type_key_up_pending_;
    char c = (!queue_empty && !key_up) ? type_queue_[type_index_] : 0;
    xSemaphoreGive(type_mutex_);

    if (queue_empty) return;

    uint8_t report[8] = {0};
    uint32_t half_delay = key_delay_ms_ / 2;

    if (key_up) {
        // Send key-up (all zeros). Retry next loop() if BLE stack queue is full.
        if (send_keyboard_input_report(conn_id_, report, 8) != ESP_OK) return;
        xSemaphoreTake(type_mutex_, portMAX_DELAY);
        type_key_up_pending_ = false;
        type_index_++;
        if (type_index_ >= type_queue_.size()) {
            type_queue_.clear();
            type_index_ = 0;
        }
        type_next_ms_ = now + half_delay;
        xSemaphoreGive(type_mutex_);
    } else {
        // Send key-down for the current character.
        if (c >= 0 && c <= 127) {
            const auto &mapping = HID_ASCII_MAP[static_cast<uint8_t>(c)];
            if (mapping.keycode != 0x00) {
                report[0] = mapping.modifier;
                report[2] = mapping.keycode;
                // Retry next loop() if BLE stack queue is full.
                if (send_keyboard_input_report(conn_id_, report, 8) != ESP_OK) return;
                xSemaphoreTake(type_mutex_, portMAX_DELAY);
                type_key_up_pending_ = true;
                type_next_ms_ = now + half_delay;
                xSemaphoreGive(type_mutex_);
                return;
            }
        }
        // Unsupported or unmapped character — skip it.
        xSemaphoreTake(type_mutex_, portMAX_DELAY);
        type_index_++;
        if (type_index_ >= type_queue_.size()) {
            type_queue_.clear();
            type_index_ = 0;
        }
        xSemaphoreGive(type_mutex_);
    }
}

static uint16_t get_keyboard_input_handle() {
    const bool report_notify_enabled = (report_ccc_val & 0x0001) != 0;
    const bool boot_notify_enabled = (boot_kb_in_ccc_val & 0x0001) != 0;

    if (proto_mode_val == 0x00) {
        if (boot_notify_enabled && s_boot_kb_input_handle != 0) {
            return s_boot_kb_input_handle;
        }
        if (report_notify_enabled && s_hid_report_handle != 0) {
            return s_hid_report_handle;
        }
    } else {
        if (report_notify_enabled && s_hid_report_handle != 0) {
            return s_hid_report_handle;
        }
        if (boot_notify_enabled && s_boot_kb_input_handle != 0) {
            return s_boot_kb_input_handle;
        }
    }

    if (s_hid_report_handle != 0) {
        return s_hid_report_handle;
    }
    return s_boot_kb_input_handle;
}

static esp_err_t send_keyboard_input_report(uint16_t conn_id, const uint8_t *report, uint16_t len) {
    const uint16_t primary_handle = get_keyboard_input_handle();
    if (primary_handle == 0) {
        ESP_LOGW(TAG, "No keyboard input handle available");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "BLE report -> handle=0x%04X mod=0x%02X key=0x%02X",
             primary_handle, report[0], report[2]);
    esp_err_t ret = esp_ble_gatts_send_indicate(s_gatts_if, conn_id, primary_handle, len, const_cast<uint8_t *>(report), false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Keyboard report send failed on handle 0x%04X (%d), caller will retry",
                 primary_handle, ret);
    }
    return ret;
}

void EspidfBleKeyboard::send_string(const std::string &str) {
    // Dedup: ESPHome API can deliver the same service call twice within ~5ms
    uint32_t now = millis();
    if (str == last_send_string_ && (now - last_send_string_ms_) < 30) {
        ESP_LOGD(TAG, "send_string dedup: \"%s\" (duplicate after %ums)", str.c_str(), now - last_send_string_ms_);
        return;
    }
    last_send_string_ = str;
    last_send_string_ms_ = now;

    ESP_LOGD(TAG, "send_string: \"%s\" (len=%u, queue=%u)",
             str.c_str(), str.size(), type_queue_.size());
    // Non-blocking: append to queue; loop() drains it one keystroke at a time.
    if (type_mutex_ == nullptr) return;
    xSemaphoreTake(type_mutex_, portMAX_DELAY);
    type_queue_ += str;
    xSemaphoreGive(type_mutex_);
}

void EspidfBleKeyboard::send_key_combo(uint8_t modifiers, uint8_t keycode) {
    // Dedup: ESPHome API can deliver the same service call twice within ~5ms
    uint32_t now = millis();
    uint16_t key_id = ((uint16_t) modifiers << 8) | keycode;
    if (key_id == last_send_key_id_ && (now - last_send_key_ms_) < 30) {
        ESP_LOGD(TAG, "send_key_combo dedup: mod=0x%02X key=0x%02X (duplicate after %ums)",
                 modifiers, keycode, now - last_send_key_ms_);
        return;
    }
    last_send_key_id_ = key_id;
    last_send_key_ms_ = now;

    ESP_LOGD(TAG, "send_key_combo: mod=0x%02X key=0x%02X", modifiers, keycode);
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    report[0] = modifiers;
    report[2] = keycode;
    send_keyboard_input_report(conn_id_, report, 8);
    vTaskDelay(pdMS_TO_TICKS(30));
    memset(report, 0, 8);
    send_keyboard_input_report(conn_id_, report, 8);
}

void EspidfBleKeyboard::send_ctrl_alt_del() {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    report[0] = 0x05; report[2] = 0x4C;
    send_keyboard_input_report(conn_id_, report, 8);
    vTaskDelay(pdMS_TO_TICKS(50));
    memset(report, 0, 8);
    send_keyboard_input_report(conn_id_, report, 8);
}

void EspidfBleKeyboard::send_sleep() {
    if (!is_connected_) return;
    uint8_t report[1] = {0x82};  // System Sleep
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, report, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t release[1] = {0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, release, false);
    ESP_LOGI(TAG, "System Sleep sent");
}

void EspidfBleKeyboard::send_shutdown() {
    if (!is_connected_) return;
    send_power();
}

void EspidfBleKeyboard::send_consumer(uint16_t usage) {
    if (!is_connected_) return;
    uint8_t report[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_consumer_report_handle, 2, report, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t release[2] = {0, 0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_consumer_report_handle, 2, release, false);
    ESP_LOGI(TAG, "Consumer report sent: 0x%04X", usage);
}

void EspidfBleKeyboard::send_power() {
    if (!is_connected_) return;
    uint8_t report[1] = {0x81};  // System Power Down
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, report, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t release[1] = {0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, release, false);
    ESP_LOGI(TAG, "System Power Down sent");
}

void EspidfBleKeyboard::send_media_play_pause() { send_consumer(0x00CD); }
void EspidfBleKeyboard::send_media_next()        { send_consumer(0x00B5); }
void EspidfBleKeyboard::send_media_prev()        { send_consumer(0x00B6); }
void EspidfBleKeyboard::send_media_stop()        { send_consumer(0x00B7); }
void EspidfBleKeyboard::send_volume_up()         { send_consumer(0x00E9); }
void EspidfBleKeyboard::send_volume_down()       { send_consumer(0x00EA); }
void EspidfBleKeyboard::send_mute()              { send_consumer(0x00E2); }

void EspidfBleKeyboard::send_mouse_click(uint8_t buttons) {
    if (!is_connected_) return;
    uint8_t report[4] = {buttons, 0, 0, 0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_mouse_report_handle, 4, report, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t release[4] = {0, 0, 0, 0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_mouse_report_handle, 4, release, false);
    ESP_LOGI(TAG, "Mouse click sent: buttons=0x%02X", buttons);
}

void EspidfBleKeyboard::send_mouse_move(int8_t x, int8_t y) {
    if (!is_connected_) return;
    uint8_t report[4] = {0, static_cast<uint8_t>(x), static_cast<uint8_t>(y), 0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_mouse_report_handle, 4, report, false);
    vTaskDelay(pdMS_TO_TICKS(20));
    uint8_t release[4] = {0, 0, 0, 0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_mouse_report_handle, 4, release, false);
    ESP_LOGD(TAG, "Mouse move sent: x=%d y=%d", x, y);
}

void EspidfBleKeyboard::send_mouse_scroll(int8_t wheel) {
    if (!is_connected_) return;
    uint8_t report[4] = {0, 0, 0, static_cast<uint8_t>(wheel)};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_mouse_report_handle, 4, report, false);
    vTaskDelay(pdMS_TO_TICKS(20));
    uint8_t release[4] = {0, 0, 0, 0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_mouse_report_handle, 4, release, false);
    ESP_LOGD(TAG, "Mouse scroll sent: wheel=%d", wheel);
}

void EspidfBleKeyboard::send_hibernate() {
    if (!is_connected_) return;
    // Win+R is a single blocking key combo (no watchdog risk).
    send_key_combo(0x08, 0x15);
    vTaskDelay(pdMS_TO_TICKS(600));
    // Queue the command text + Enter through the non-blocking state machine.
    send_string("shutdown /h\n");
}

// ── Centralized action executor ──────────────────────────────────

void EspidfBleKeyboard::execute_action(const std::string &action) {
    // Delay action for multi-step macros
    if (action.find("delay:") == 0) {
        int ms = 0;
        if (sscanf(action.c_str(), "delay:%i", &ms) == 1 && ms > 0 && ms <= 10000)
            vTaskDelay(pdMS_TO_TICKS(ms));
        return;
    }
    // Parametric actions
    if (action.find("combo:") == 0) {
        int mod = 0, key = 0;
        if (sscanf(action.c_str(), "combo:%i:%i", &mod, &key) == 2)
            send_key_combo((uint8_t) mod, (uint8_t) key);
        return;
    }
    if (action.find("consumer:") == 0) {
        int usage = 0;
        if (sscanf(action.c_str(), "consumer:%i", &usage) == 1)
            send_consumer((uint16_t) usage);
        return;
    }
    if (action.find("mouse_click:") == 0) {
        int buttons = 0;
        if (sscanf(action.c_str(), "mouse_click:%i", &buttons) == 1)
            send_mouse_click((uint8_t) buttons);
        return;
    }
    if (action.find("mouse_move:") == 0) {
        int x = 0, y = 0;
        if (sscanf(action.c_str(), "mouse_move:%i:%i", &x, &y) == 2)
            send_mouse_move((int8_t) x, (int8_t) y);
        return;
    }
    if (action.find("mouse_scroll:") == 0) {
        int wheel = 0;
        if (sscanf(action.c_str(), "mouse_scroll:%i", &wheel) == 1)
            send_mouse_scroll((int8_t) wheel);
        return;
    }
    if (action.find("switch_host:") == 0) {
        int slot = 0;
        if (sscanf(action.c_str(), "switch_host:%i", &slot) == 1)
            switch_host((uint8_t) slot);
        return;
    }
    if (action.find("forget_host:") == 0) {
        int slot = 0;
        if (sscanf(action.c_str(), "forget_host:%i", &slot) == 1)
            forget_host((uint8_t) slot);
        return;
    }

    // Named actions
    if (action == "ctrl_alt_del")      send_ctrl_alt_del();
    else if (action == "sleep")        send_sleep();
    else if (action == "shutdown")     send_shutdown();
    else if (action == "hibernate")    send_hibernate();
    else if (action == "power")        send_power();
    else if (action == "play_pause")   send_media_play_pause();
    else if (action == "next_track")   send_media_next();
    else if (action == "prev_track")   send_media_prev();
    else if (action == "stop")         send_media_stop();
    else if (action == "volume_up")    send_volume_up();
    else if (action == "volume_down")  send_volume_down();
    else if (action == "mute")         send_mute();
    else if (action == "left_click")   send_mouse_click(0x01);
    else if (action == "right_click")  send_mouse_click(0x02);
    else if (action == "middle_click") send_mouse_click(0x04);
    else send_string(action);  // Fallback: send as typed text
}

bool EspidfBleKeyboard::execute_macro(uint8_t index) {
    if (index >= macros_.size()) return false;
    const std::string &action = macros_[index].action;
    // Multi-step macros: split on '|' and execute each step
    size_t start = 0;
    while (start < action.size()) {
        size_t end = action.find('|', start);
        if (end == std::string::npos) end = action.size();
        std::string step = action.substr(start, end - start);
        // Trim whitespace
        while (!step.empty() && step.front() == ' ') step.erase(step.begin());
        while (!step.empty() && step.back() == ' ') step.pop_back();
        if (!step.empty()) {
            execute_action(step);
        }
        start = end + 1;
        // Small default delay between steps (unless step was an explicit delay)
        if (start < action.size() && step.find("delay:") != 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    return true;
}

void EspidfBleKeyboardButton::press_action() {
    if (!parent_) return;
    parent_->execute_action(action_);
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome




