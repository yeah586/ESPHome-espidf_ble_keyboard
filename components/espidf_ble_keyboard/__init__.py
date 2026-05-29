import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import automation

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["sensor", "binary_sensor", "button", "text"]

# Define configuration keys
CONF_DEVICE_NAME = "device_name"
CONF_KEY_DELAY_MS = "key_delay_ms"
CONF_PASSKEY = "passkey"
CONF_PASSKEY_MODE = "passkey_mode"
CONF_WEB_CONTROL = "web_control"
CONF_HOST_SLOTS = "host_slots"
CONF_MOUSE_SENSITIVITY = "mouse_sensitivity"
CONF_MOUSE_ACCEL = "mouse_acceleration"
CONF_MOUSE_MAX_SPEED = "mouse_max_speed"
CONF_SCROLL_SENSITIVITY = "scroll_sensitivity"
CONF_SCREEN_WIDTH = "screen_width"
CONF_SCREEN_HEIGHT = "screen_height"
CONF_MONITORS = "monitors"
CONF_WIDTH = "width"
CONF_HEIGHT = "height"
CONF_X = "x"
CONF_Y = "y"
CONF_NAME = "name"
CONF_PRIMARY = "primary"
CONF_HOSTS = "hosts"
CONF_SLOT = "slot"
CONF_CUSTOM_TEXT_ID = "custom_text_id"
CONF_KEYBOARD_LAYOUT = "keyboard_layout"
CONF_LAYOUT = "layout"
PASSKEY_MODE_LEGACY = "legacy"
PASSKEY_MODE_SECURE_CONNECTIONS = "secure_connections"

# To add a layout: register it in keyboard_layouts.cpp, add its id JS rows in
# web_control.cpp, then append the id here.
SUPPORTED_LAYOUTS = ["us", "uk", "de"]

espidf_ble_keyboard_ns = cg.esphome_ns.namespace("espidf_ble_keyboard")
EspidfBleKeyboard = espidf_ble_keyboard_ns.class_("EspidfBleKeyboard", cg.Component)
RssiAboveTrigger = espidf_ble_keyboard_ns.class_("RssiAboveTrigger", automation.Trigger.template(cg.int_))
RssiBelowTrigger = espidf_ble_keyboard_ns.class_("RssiBelowTrigger", automation.Trigger.template(cg.int_))

CONF_ON_RSSI_ABOVE = "on_rssi_above"
CONF_ON_RSSI_BELOW = "on_rssi_below"
CONF_THRESHOLD = "threshold"


def _web_control_schema(config):
    """When web_control is true, require web_server_base."""
    if config.get(CONF_WEB_CONTROL):
        from esphome.components import web_server_base
        from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
        config = cv.Schema({
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
        }, extra=cv.ALLOW_EXTRA)(config)
    return config


# Absolute-pointer monitor region (virtual-desktop pixels)
MONITOR_SCHEMA = cv.Schema({
    cv.Optional(CONF_NAME): cv.string,
    cv.Required(CONF_X): cv.int_,
    cv.Required(CONF_Y): cv.int_,
    cv.Required(CONF_WIDTH): cv.int_range(min=1),
    cv.Required(CONF_HEIGHT): cv.int_range(min=1),
    # Mark the Windows primary monitor (its top-left = Windows 0,0). Lets the web
    # Position Finder emit mouse_goto values; only one should be primary.
    cv.Optional(CONF_PRIMARY, default=False): cv.boolean,
})

HOST_SCHEMA = cv.Schema({
    cv.Required(CONF_SLOT): cv.int_range(min=0, max=9),
    cv.Optional(CONF_PASSKEY): cv.int_range(min=0, max=999999),
    cv.Optional(CONF_PASSKEY_MODE, default=PASSKEY_MODE_LEGACY): cv.one_of(
        PASSKEY_MODE_LEGACY,
        PASSKEY_MODE_SECURE_CONNECTIONS,
        lower=True,
    ),
    cv.Optional(CONF_LAYOUT): cv.one_of(*SUPPORTED_LAYOUTS, lower=True),
})

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(EspidfBleKeyboard),
        cv.Optional(CONF_DEVICE_NAME, default="ESP32 BLE KB"): cv.All(cv.string, cv.Length(max=29)),
        cv.Optional(CONF_KEY_DELAY_MS, default=80): cv.int_range(min=2, max=10000),
        cv.Optional(CONF_PASSKEY): cv.int_range(min=0, max=999999),
        cv.Optional(CONF_PASSKEY_MODE, default=PASSKEY_MODE_LEGACY): cv.one_of(
            PASSKEY_MODE_LEGACY,
            PASSKEY_MODE_SECURE_CONNECTIONS,
            lower=True,
        ),
        cv.Optional(CONF_WEB_CONTROL, default=False): cv.boolean,
        cv.Optional(CONF_MOUSE_SENSITIVITY, default=1.0): cv.float_range(min=0.1, max=10.0),
        cv.Optional(CONF_MOUSE_ACCEL, default=0.15): cv.float_range(min=0.0, max=2.0),
        cv.Optional(CONF_MOUSE_MAX_SPEED, default=4.0): cv.float_range(min=0.5, max=20.0),
        cv.Optional(CONF_SCROLL_SENSITIVITY, default=2.0): cv.float_range(min=0.1, max=10.0),
        cv.Optional(CONF_SCREEN_WIDTH, default=1920): cv.int_range(min=1, max=32767),
        cv.Optional(CONF_SCREEN_HEIGHT, default=1080): cv.int_range(min=1, max=32767),
        cv.Optional(CONF_MONITORS): cv.ensure_list(MONITOR_SCHEMA),
        cv.Optional(CONF_KEYBOARD_LAYOUT, default="us"): cv.one_of(*SUPPORTED_LAYOUTS, lower=True),
        cv.Optional(CONF_CUSTOM_TEXT_ID): cv.ensure_list(cv.use_id(cg.EntityBase)),
        cv.Optional(CONF_HOST_SLOTS, default=4): cv.int_range(min=1, max=10),
        cv.Optional(CONF_HOSTS): cv.All(cv.ensure_list(HOST_SCHEMA)),
        cv.Optional(CONF_ON_RSSI_ABOVE): automation.validate_automation({
            cv.GenerateID(automation.CONF_TRIGGER_ID): cv.declare_id(RssiAboveTrigger),
            cv.Required(CONF_THRESHOLD): cv.int_range(min=-127, max=0),
        }),
        cv.Optional(CONF_ON_RSSI_BELOW): automation.validate_automation({
            cv.GenerateID(automation.CONF_TRIGGER_ID): cv.declare_id(RssiBelowTrigger),
            cv.Required(CONF_THRESHOLD): cv.int_range(min=-127, max=0),
        }),
    }).extend(cv.COMPONENT_SCHEMA),
    _web_control_schema,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
    cg.add(var.set_key_delay_ms(config[CONF_KEY_DELAY_MS]))

    if CONF_PASSKEY in config:
        cg.add(var.set_passkey(config[CONF_PASSKEY]))

    cg.add(var.set_passkey_secure_connections(
        config[CONF_PASSKEY_MODE] == PASSKEY_MODE_SECURE_CONNECTIONS
    ))

    cg.add(var.set_host_slots(config[CONF_HOST_SLOTS]))
    cg.add(var.set_mouse_sensitivity(config[CONF_MOUSE_SENSITIVITY]))
    cg.add(var.set_mouse_accel(config[CONF_MOUSE_ACCEL]))
    cg.add(var.set_mouse_max_speed(config[CONF_MOUSE_MAX_SPEED]))
    cg.add(var.set_scroll_sensitivity(config[CONF_SCROLL_SENSITIVITY]))
    cg.add(var.set_screen_size(config[CONF_SCREEN_WIDTH], config[CONF_SCREEN_HEIGHT]))
    for mon in config.get(CONF_MONITORS, []):
        cg.add(var.add_monitor(mon[CONF_X], mon[CONF_Y], mon[CONF_WIDTH], mon[CONF_HEIGHT], mon[CONF_PRIMARY]))
    cg.add(var.set_keyboard_layout(config[CONF_KEYBOARD_LAYOUT]))

    if CONF_HOSTS in config:
        for host in config[CONF_HOSTS]:
            if CONF_PASSKEY in host:
                sc = host[CONF_PASSKEY_MODE] == PASSKEY_MODE_SECURE_CONNECTIONS
                cg.add(var.set_host_slot_passkey(host[CONF_SLOT], host[CONF_PASSKEY], sc))
            if CONF_LAYOUT in host:
                cg.add(var.set_host_slot_layout(host[CONF_SLOT], host[CONF_LAYOUT]))

    if CONF_CUSTOM_TEXT_ID in config:
        cg.add_define("USE_TEXT")
        for i, text_id in enumerate(config[CONF_CUSTOM_TEXT_ID]):
            text_entity = await cg.get_variable(text_id)
            cg.add(var.add_custom_text(text_entity))
            cg.add(var.register_button(f"Send {text_id.id}", f"send_custom_text:{i}"))

    if config[CONF_WEB_CONTROL]:
        cg.add_define("USE_BLE_KEYBOARD_WEB_CONTROL")
        cg.add(var.set_web_control(True))
        from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
        base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
        cg.add(var.set_web_server_base(base))

    # set_setup_priority() removed in ESPHome 2026.x
    # Priority is now set via get_setup_priority() override in the C++ header

    for conf in config.get(CONF_ON_RSSI_ABOVE, []):
        trigger = cg.new_Pvariable(conf[automation.CONF_TRIGGER_ID], var, conf[CONF_THRESHOLD])
        await automation.build_automation(trigger, [(cg.int_, "rssi")], conf)

    for conf in config.get(CONF_ON_RSSI_BELOW, []):
        trigger = cg.new_Pvariable(conf[automation.CONF_TRIGGER_ID], var, conf[CONF_THRESHOLD])
        await automation.build_automation(trigger, [(cg.int_, "rssi")], conf)

    try:
        from esphome.components.esp32 import include_builtin_idf_component
        include_builtin_idf_component("bt")
        include_builtin_idf_component("nvs_flash")
    except ImportError:
        pass