import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PERCENT,
)

from . import Coway250SComponent

DEPENDENCIES = ["coway_250s"]

CONF_COWAY_250S_ID = "coway_250s_id"
SENSORS = {
    "power": {},
    "operating_mode": {},
    "fan_state": {},
    "panel_lighting": {},
    "off_timer": {},
    "sensitivity": {},
    "button_lock": {},
    "pm25": {
        "device_class": DEVICE_CLASS_PM25,
        "unit_of_measurement": UNIT_MICROGRAMS_PER_CUBIC_METER,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "pm10": {
        "device_class": DEVICE_CLASS_PM10,
        "unit_of_measurement": UNIT_MICROGRAMS_PER_CUBIC_METER,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "ambient_light": {"state_class": STATE_CLASS_MEASUREMENT},
    "prefilter_remaining": {
        "unit_of_measurement": UNIT_PERCENT,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "max2_filter_remaining": {
        "unit_of_measurement": UNIT_PERCENT,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
}

schema = {cv.GenerateID(CONF_COWAY_250S_ID): cv.use_id(Coway250SComponent)}
for key, defaults in SENSORS.items():
    schema[cv.Optional(key)] = sensor.sensor_schema(accuracy_decimals=0, **defaults)

CONFIG_SCHEMA = cv.Schema(schema)


async def to_code(config) -> None:
    component = await cg.get_variable(config[CONF_COWAY_250S_ID])
    for key in SENSORS:
        if sensor_config := config.get(key):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(component, f"set_{key}_sensor")(sens))
