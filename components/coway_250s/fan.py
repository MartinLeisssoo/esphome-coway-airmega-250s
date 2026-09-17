import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan
from esphome.const import CONF_ID

from . import Coway250SComponent, coway_250s_ns

DEPENDENCIES = ["coway_250s"]
CowayFan = coway_250s_ns.class_("CowayFan", cg.Component, fan.Fan)
CONF_COWAY_ID = "coway_250s_id"
CONFIG_SCHEMA = fan.fan_schema(CowayFan, default_restore_mode="NO_RESTORE").extend({
  cv.GenerateID(CONF_COWAY_ID): cv.use_id(Coway250SComponent),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
  parent = await cg.get_variable(config[CONF_COWAY_ID])
  var = cg.new_Pvariable(config[CONF_ID], parent)
  await cg.register_component(var, config)
  await fan.register_fan(var, config)
