import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import EspidfBleKeyboard

DEPENDENCIES = ["espidf_ble_keyboard"]

CONF_KEYBOARD_ID = "keyboard_id"

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    parent = await cg.get_variable(config[CONF_KEYBOARD_ID])
    cg.add(parent.set_paired_binary_sensor(var))
