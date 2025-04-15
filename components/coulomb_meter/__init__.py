import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    UNIT_PERCENT,
)

CODEOWNERS = ["SqrTT"]

CONF_CAPACITY_AH = "capacity_ah"
CONF_FULLCHARGE_VOLTAGE = "fullcharge_voltage"
CONF_FULLCHARGE_CURRENT = "fullcharge_current"
CONF_FULLCHARGE_TIME = "fullcharge_time"

CONF_DISCHARGE_VOLTAGE = "discharge_voltage"
CONF_DISCHARGE_TIME = "discharge_time"

CONF_SOC_SENSOR = "soc_sensor"

COULOMB_SCHEMA = cv.Schema({
    cv.Required(CONF_FULLCHARGE_VOLTAGE): cv.All(cv.voltage, cv.Range(min=0.0)),
    cv.Required(CONF_FULLCHARGE_CURRENT): cv.All(cv.current, cv.Range(min=0.0)),
    cv.Required(CONF_FULLCHARGE_TIME): cv.All(cv.positive_time_period_seconds),

    cv.Required(CONF_DISCHARGE_VOLTAGE): cv.All(cv.voltage, cv.Range(min=0.0)),
    cv.Required(CONF_DISCHARGE_TIME): cv.All(cv.positive_time_period_seconds),
    cv.Required(CONF_CAPACITY_AH): cv.All(cv.int_range(min=0)),

    cv.Required(CONF_SOC_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
    ),
})
coulomb_meter_ns = cg.esphome_ns.namespace("coulomb_meter")
CoulombMeter_ns = coulomb_meter_ns.class_(
    "CoulombMeter", cg.PollingComponent
)

async def setup_coulomb(var, config):
    # await cg.register_component(var, config)

    cg.add(var.set_fully_charge_voltage(config[CONF_FULLCHARGE_VOLTAGE]))
    cg.add(var.set_fully_charge_current(config[CONF_FULLCHARGE_CURRENT]))
    cg.add(var.set_fully_charge_time(config[CONF_FULLCHARGE_TIME]))
    cg.add(var.set_fully_discharge_voltage(config[CONF_DISCHARGE_VOLTAGE]))
    cg.add(var.set_fully_discharge_time(config[CONF_DISCHARGE_TIME]))
    cg.add(var.set_full_capacity(config[CONF_CAPACITY_AH]))

    if conf := config.get(CONF_SOC_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_soc_target_sensor(sens))

CONFIG_SCHEMA = cv.Schema({})
    

   