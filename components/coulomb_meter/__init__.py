import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    UNIT_PERCENT,
    ICON_TIMER,
    UNIT_MINUTE,
    UNIT_WATT_HOURS,
    STATE_CLASS_TOTAL_INCREASING,
    STATE_CLASS_TOTAL
)

CODEOWNERS = ["SqrTT"]

CONF_CAPACITY_AH = "capacity_ah"
CONF_ENERGY_FULL = "energy_full_wh"
CONF_FULLCHARGE_VOLTAGE = "fullcharge_voltage"
CONF_FULLCHARGE_CURRENT = "fullcharge_current"
CONF_FULLCHARGE_TIME = "fullcharge_time"

CONF_DISCHARGE_VOLTAGE = "discharge_voltage"
CONF_DISCHARGE_TIME = "discharge_time"

UNIT_AMPERE_HOURS = "Ah"

CONF_CHARGE_LEVEL_SENSOR = "charge_level_sensor"
CONF_CHARGE_IN_SENSOR = "charge_in_sensor"
CONF_CHARGE_OUT_SENSOR = "charge_out_sensor"
CONF_CHARGE_REMAINING_SENSOR = "charge_remaining_sensor"
CONF_CHARGE_CALCULATED_SENSOR = "charge_calculated_sensor"

CONF_ENERGY_LEVEL_SENSOR = "energy_level_sensor"
CONF_ENERGY_REMAINING_SENSOR = "energy_remaining_sensor"
CONF_ENERGY_IN_SENSOR = "energy_in_sensor"
CONF_ENERGY_OUT_SENSOR = "energy_out_sensor"
CONF_ENERGY_CALCULATED_SENSOR = "energy_calculated_sensor"

CONF_CHARGE_TIME_REMAINING_SENSOR = "charge_time_remaining_sensor"
CONF_DISCHARGE_TIME_REMAINING_SENSOR = "discharge_time_remaining_sensor"


COULOMB_SCHEMA = cv.Schema({
    cv.Required(CONF_FULLCHARGE_VOLTAGE): cv.All(cv.voltage, cv.Range(min=0.0)),
    cv.Required(CONF_FULLCHARGE_CURRENT): cv.All(cv.current, cv.Range(min=0.0)),
    cv.Required(CONF_FULLCHARGE_TIME): cv.All(cv.positive_time_period_seconds),

    cv.Required(CONF_DISCHARGE_VOLTAGE): cv.All(cv.voltage, cv.Range(min=0.0)),
    cv.Required(CONF_DISCHARGE_TIME): cv.All(cv.positive_time_period_seconds),
    cv.Required(CONF_CAPACITY_AH): cv.All(cv.float_range(min=0)),
    cv.Required(CONF_ENERGY_FULL): cv.All(cv.float_range(min=0)),


    cv.Optional(CONF_CHARGE_TIME_REMAINING_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_MINUTE,
        accuracy_decimals=0,
        icon=ICON_TIMER
    ),
    cv.Optional(CONF_DISCHARGE_TIME_REMAINING_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_MINUTE,
        accuracy_decimals=0,
        icon=ICON_TIMER
    ),

    ### amps sensors
    cv.Optional(CONF_CHARGE_LEVEL_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
    ),

    cv.Optional(CONF_CHARGE_CALCULATED_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE_HOURS,
        accuracy_decimals=3
    ),
    cv.Optional(CONF_CHARGE_IN_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE_HOURS,
        accuracy_decimals=3,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    cv.Optional(CONF_CHARGE_OUT_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE_HOURS,
        accuracy_decimals=3,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    cv.Optional(CONF_CHARGE_REMAINING_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE_HOURS,
        accuracy_decimals=3,
        state_class=STATE_CLASS_TOTAL,
    ),
    #### energy sensor
    cv.Optional(CONF_ENERGY_LEVEL_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL,
    ),
    cv.Optional(CONF_ENERGY_REMAINING_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT_HOURS,
        accuracy_decimals=3,
        state_class=STATE_CLASS_TOTAL,
    ),
    cv.Optional(CONF_ENERGY_IN_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT_HOURS,
        accuracy_decimals=3,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    cv.Optional(CONF_ENERGY_OUT_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT_HOURS,
        accuracy_decimals=3,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    cv.Optional(CONF_ENERGY_CALCULATED_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT_HOURS,
        accuracy_decimals=3
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
    cg.add(var.set_full_energy(config[CONF_ENERGY_FULL]))

    if conf := config.get(CONF_DISCHARGE_TIME_REMAINING_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_discharge_time_remaining_sensor(sens))

    if conf := config.get(CONF_CHARGE_TIME_REMAINING_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_time_remaining_sensor(sens))

    if conf := config.get(CONF_CHARGE_LEVEL_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_level_sensor(sens))

    if conf := config.get(CONF_CHARGE_IN_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_in_sensor(sens))

    if conf := config.get(CONF_CHARGE_OUT_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_out_sensor(sens))

    if conf := config.get(CONF_CHARGE_REMAINING_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_remaining_sensor(sens))

    if conf := config.get(CONF_CHARGE_CALCULATED_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_calculated_sensor(sens))
    
    ### WH sensors
    if conf := config.get(CONF_ENERGY_LEVEL_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_level_sensor(sens))

    if conf := config.get(CONF_ENERGY_REMAINING_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_remaining_sensor(sens))
    
    if conf := config.get(CONF_ENERGY_IN_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_in_sensor(sens))
    
    if conf := config.get(CONF_ENERGY_OUT_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_out_sensor(sens))

    if conf := config.get(CONF_ENERGY_CALCULATED_SENSOR):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_calculated_sensor(sens))

CONFIG_SCHEMA = cv.Schema({})
    

   