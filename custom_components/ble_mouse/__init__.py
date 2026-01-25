import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_NAME

DEPENDENCIES = ["esp32"]

ble_mouse_ns = cg.esphome_ns.namespace("ble_mouse")
BleMouseComponent = ble_mouse_ns.class_("BleMouseComponent", cg.Component)

MouseMoveAction = ble_mouse_ns.class_("MouseMoveAction", automation.Action)
MouseClickAction = ble_mouse_ns.class_("MouseClickAction", automation.Action)

CONF_MANUFACTURER = "manufacturer"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BleMouseComponent),
        cv.Optional(CONF_NAME, default="ESPHome AirMouse"): cv.string,
        cv.Optional(CONF_MANUFACTURER, default="Espressif"): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_device_name(config[CONF_NAME]))
    cg.add(var.set_manufacturer(config[CONF_MANUFACTURER]))
    
    # Add libraries
    cg.add_library("NimBLE-Arduino", "1.4.1")
    cg.add_library("ESP32 BLE Mouse", "0.3.1")
    cg.add_build_flag("-DUSE_NIMBLE")

@automation.register_action(
    "ble_mouse.move",
    MouseMoveAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BleMouseComponent),
            cv.Optional("x", default=0): cv.templatable(cv.int_),
            cv.Optional("y", default=0): cv.templatable(cv.int_),
            cv.Optional("wheel", default=0): cv.templatable(cv.int_),
            cv.Optional("h_wheel", default=0): cv.templatable(cv.int_),
        }
    ),
)
async def ble_mouse_move_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    await cg.templatable(var.set_x, config["x"], args)
    await cg.templatable(var.set_y, config["y"], args)
    await cg.templatable(var.set_wheel, config["wheel"], args)
    await cg.templatable(var.set_h_wheel, config["h_wheel"], args)
    return var

@automation.register_action(
    "ble_mouse.click",
    MouseClickAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BleMouseComponent),
            cv.Optional("button", default=1): cv.templatable(cv.int_),
        }
    ),
)
async def ble_mouse_click_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    await cg.templatable(var.set_button, config["button"], args)
    return var
