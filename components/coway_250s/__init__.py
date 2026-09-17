import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["uart"]

coway_250s_ns = cg.esphome_ns.namespace("coway_250s")
Coway250SComponent = coway_250s_ns.class_(
    "Coway250SComponent", uart.UARTDevice, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(Coway250SComponent)})
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
