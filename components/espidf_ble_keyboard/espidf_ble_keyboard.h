#pragma once
#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <atomic>
#include <string>
#include <vector>

#include "freertos/semphr.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "nvs_flash.h"

#ifdef USE_BLE_KEYBOARD_WEB_CONTROL
#include "esphome/components/web_server_base/web_server_base.h"
#include "web_control.h"
#endif

namespace esphome {
namespace espidf_ble_keyboard {

// Maximum number of host slots for multi-host switching
static const uint8_t MAX_HOST_SLOTS = 4;

class EspidfBleKeyboard : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return -200.0f; }
  void send_string(const std::string &str);
  void send_ctrl_alt_del();
  void send_key_combo(uint8_t modifiers, uint8_t keycode);
  void send_sleep();
  void send_shutdown();
  void send_hibernate();
  void send_consumer(uint16_t usage);
  void send_power();
  void send_media_play_pause();
  void send_media_next();
  void send_media_prev();
  void send_media_stop();
  void send_volume_up();
  void send_volume_down();
  void send_mute();
  void send_mouse_click(uint8_t buttons);
  void send_mouse_move(int8_t x, int8_t y);
  void send_mouse_scroll(int8_t wheel);

  void set_passkey(uint32_t passkey) {
    passkey_ = passkey;
    has_passkey_ = true;
  }
  void set_passkey_secure_connections(bool enabled) { passkey_secure_connections_ = enabled; }
  bool has_passkey() const { return has_passkey_; }
  uint32_t passkey() const { return passkey_; }
  bool passkey_secure_connections() const { return passkey_secure_connections_; }

  void set_device_name(const std::string &name) { device_name_ = name; }
  const std::string &device_name() const { return device_name_; }

  void set_key_delay_ms(uint32_t ms) { key_delay_ms_ = ms; }
  uint32_t key_delay_ms() const { return key_delay_ms_; }

  void set_web_control(bool enabled) { web_control_enabled_ = enabled; }
  void set_host_slots(uint8_t slots) { host_slots_ = slots > MAX_HOST_SLOTS ? MAX_HOST_SLOTS : slots; }

  struct ButtonInfo {
    std::string name;
    std::string action;
  };
  void register_button(const std::string &name, const std::string &action) {
    buttons_.push_back({name, action});
  }
  const std::vector<ButtonInfo> &get_buttons() const { return buttons_; }

#ifdef USE_BLE_KEYBOARD_WEB_CONTROL
  void set_web_server_base(web_server_base::WebServerBase *base) { web_server_base_ = base; }
#endif

  void set_paired_binary_sensor(binary_sensor::BinarySensor *sensor) {
    paired_binary_sensor_ = sensor;
    if (paired_binary_sensor_ != nullptr) {
      paired_binary_sensor_->publish_state(is_paired_);
    }
  }

  void set_paired(bool paired) {
    is_paired_ = paired;
    if (paired_binary_sensor_ != nullptr) {
      paired_binary_sensor_->publish_state(paired);
    }
  }
  bool is_paired() const { return is_paired_; }

  void queue_paired_state(bool paired) {
    pending_paired_state_.store(paired);
    pending_paired_update_.store(true);
  }

  void set_connected(bool connected, uint16_t conn_id) {
    is_connected_ = connected;
    conn_id_ = conn_id;
  }
  bool is_connected() const { return is_connected_; }
  uint16_t conn_id() const { return conn_id_; }

  // Multi-host switching
  void switch_host(uint8_t slot);
  void forget_host(uint8_t slot);
  uint8_t active_host_slot() const { return active_slot_; }
  uint8_t host_slots() const { return host_slots_; }

  struct HostSlot {
    bool occupied{false};
    esp_bd_addr_t addr{};
    esp_ble_addr_type_t addr_type{BLE_ADDR_TYPE_PUBLIC};
    std::string name;  // friendly label
  };
  const HostSlot &get_host_slot(uint8_t slot) const { return hosts_[slot]; }

 protected:
  bool is_connected_{false};
  uint16_t conn_id_{0};
  bool is_paired_{false};
  std::atomic<bool> pending_paired_update_{false};
  std::atomic<bool> pending_paired_state_{false};
  binary_sensor::BinarySensor *paired_binary_sensor_{nullptr};
  uint32_t passkey_{0};
  bool has_passkey_{false};
  bool passkey_secure_connections_{false};
  std::string device_name_{"ESP32 BLE KB"};
  uint32_t key_delay_ms_{80};

  bool web_control_enabled_{false};
  std::vector<ButtonInfo> buttons_;

  // Multi-host state
  uint8_t host_slots_{MAX_HOST_SLOTS};
  uint8_t active_slot_{0};
  HostSlot hosts_[MAX_HOST_SLOTS];
  void save_host_slots_();
  void load_host_slots_();
  void assign_host_slot_(uint8_t slot, const esp_bd_addr_t addr, esp_ble_addr_type_t addr_type);

#ifdef USE_BLE_KEYBOARD_WEB_CONTROL
  web_server_base::WebServerBase *web_server_base_{nullptr};
  BleKeyboardWebControl *web_control_{nullptr};
#endif

  // Non-blocking string typing state machine (driven from loop())
  SemaphoreHandle_t type_mutex_{nullptr};
  std::string type_queue_;
  size_t type_index_{0};
  bool type_key_up_pending_{false};
  uint32_t type_next_ms_{0};
};

class EspidfBleKeyboardButton : public button::Button, public Component {
 public:
  void set_parent(EspidfBleKeyboard *parent) { parent_ = parent; }
  void press_action() override;
  void set_action(const std::string &action) { action_ = action; }
  float get_setup_priority() const override { return -200.0f; }
 protected:
  EspidfBleKeyboard *parent_{nullptr};
  std::string action_;
};

}  // namespace espidf_ble_keyboard
}  // namespace esphome