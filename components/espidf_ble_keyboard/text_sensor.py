import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import EspidfBleKeyboard

DEPENDENCIES = ["espidf_ble_keyboard"]

CONF_KEYBOARD_ID = "keyboard_id"
CONF_TYPE = "type"

TYPE_HIDDEN_BUTTONS = "hidden_buttons"

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
        cv.Optional(CONF_TYPE, default=TYPE_HIDDEN_BUTTONS): cv.one_of(
            TYPE_HIDDEN_BUTTONS, lower=True
        ),
    }
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    parent = await cg.get_variable(config[CONF_KEYBOARD_ID])
    cg.add(parent.set_hidden_sensor(var))
