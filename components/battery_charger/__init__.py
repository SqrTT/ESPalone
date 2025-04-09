CODEOWNERS = ["SqrTT"]

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (sensor, text_sensor)
from esphome.const import (
    CONF_ID,
    UNIT_VOLT
    
)
CODEOWNERS = ["SqrTT"]


CONF_SENSOR_VOLTAGE_ID = 'voltage_sensor'
CONF_TARGET_SENSOR_VOLTAGE = 'target_voltage_sensor'
CONF_TARGET_SENSOR_CHARGE_STATE = 'charge_state_sensor'
CONF_FLOAT_VOLTAGE_ID = 'float_voltage'
CONF_SENSOR_CURRENT_ID = 'current_sensor'

CONF_VOLTAGE_MAX = 'voltage_max'
CONF_VOLTAGE_MIN = 'voltage_min'
CONF_VOLTAGE_RECOVERY_DELAY = 'voltage_auto_recovery_delay'

CONF_ABSORPTION_TIME = 'absorption_time'
CONF_ABSORPTION_VOLTAGE = 'absorption_voltage'
CONF_ABSORPTION_CURRENT = 'absorption_current'
CONF_ABSORPTION_RESTART_TIME = 'absorption_restart_time'
CONF_ABSORPTION_RESTART_VOLTAGE = 'absorption_restart_voltage'
CONF_ABSORPTION_LOW_VOLTAGE_DELAY = 'absorption_low_voltage_delay'

CONF_EQUAIZATION_VOLTAGE = 'equaization_voltage'
CONF_EQUAIZATION_TIME = 'equaization_time'
CONF_EQUAIZATION_INTERVAL = 'equaization_interval'
CONF_EQUAIZATION_TIMEOUT = 'equaization_timeout'

charger_ns = cg.esphome_ns.namespace("battery_charger")
ChargerComponent = charger_ns.class_(
    "ChargerComponent", cg.Component, text_sensor.TextSensor
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ChargerComponent),
            cv.Required(CONF_SENSOR_VOLTAGE_ID): cv.use_id(sensor.Sensor),
            cv.Required(CONF_FLOAT_VOLTAGE_ID): cv.voltage,
            cv.Optional(CONF_TARGET_SENSOR_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
            ),
            cv.Optional(CONF_TARGET_SENSOR_CHARGE_STATE): cv.use_id(text_sensor.TextSensor),
            cv.Optional(CONF_SENSOR_CURRENT_ID): cv.use_id(sensor.Sensor),

            cv.Optional(CONF_VOLTAGE_MAX): cv.voltage,
            cv.Optional(CONF_VOLTAGE_MIN): cv.voltage,
            cv.Optional(CONF_VOLTAGE_RECOVERY_DELAY, default='0s'): cv.positive_time_period_seconds,

            cv.Optional(CONF_ABSORPTION_VOLTAGE): cv.voltage,
            cv.Optional(CONF_ABSORPTION_RESTART_VOLTAGE): cv.voltage,
            cv.Optional(CONF_ABSORPTION_RESTART_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_ABSORPTION_CURRENT): cv.current,
            cv.Optional(CONF_ABSORPTION_TIME): cv.positive_time_period_seconds,
            cv.Optional(CONF_ABSORPTION_LOW_VOLTAGE_DELAY, default= '1min'): cv.positive_time_period_seconds,

            cv.Optional(CONF_EQUAIZATION_VOLTAGE): cv.voltage,
            cv.Optional(CONF_EQUAIZATION_TIME, default= "1h"): cv.positive_time_period_seconds,
            cv.Optional(CONF_EQUAIZATION_INTERVAL, default="7d"): cv.positive_time_period_seconds,
            cv.Optional(CONF_EQUAIZATION_TIMEOUT, default= '3h'): cv.positive_time_period_seconds,

        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sens = await cg.get_variable(config[CONF_SENSOR_VOLTAGE_ID])
    cg.add(var.set_voltage_sensor(sens))

    if config.get(CONF_SENSOR_CURRENT_ID) is not None:
        sensCurrent = await cg.get_variable(config[CONF_SENSOR_CURRENT_ID])
        cg.add(var.set_current_sensor(sensCurrent))

    if CONF_TARGET_SENSOR_CHARGE_STATE in config:
        sens = await cg.get_variable(config[CONF_TARGET_SENSOR_CHARGE_STATE])
        cg.add(var.set_charge_state_sensor(sens))
    
    if CONF_VOLTAGE_MAX in config:
        cg.add(var.set_max_voltage(config[CONF_VOLTAGE_MAX]))

    if CONF_VOLTAGE_MIN in config:
        cg.add(var.set_min_voltage(config[CONF_VOLTAGE_MIN]))

    if CONF_VOLTAGE_RECOVERY_DELAY in config and config[CONF_VOLTAGE_RECOVERY_DELAY].seconds > 0:
        cg.add(var.set_voltage_auto_recovery_delay(config[CONF_VOLTAGE_RECOVERY_DELAY]))

    cg.add(var.set_float_voltage(config[CONF_FLOAT_VOLTAGE_ID])) 

    if config.get(CONF_ABSORPTION_VOLTAGE) is not None:
        cg.add(var.set_absorption_voltage(config[CONF_ABSORPTION_VOLTAGE])) 

        if config.get(CONF_ABSORPTION_CURRENT) is not None:
            cg.add(var.set_absorption_current(config[CONF_ABSORPTION_CURRENT]))

        if config.get(CONF_ABSORPTION_TIME) is not None:
            cg.add(var.set_absorption_time(config[CONF_ABSORPTION_TIME]))

        if config.get(CONF_ABSORPTION_RESTART_VOLTAGE) is not None:
            cg.add(var.set_absorption_restart_voltage(config[CONF_ABSORPTION_RESTART_VOLTAGE]))

        if config.get(CONF_ABSORPTION_RESTART_TIME) is not None:
            cg.add(var.set_absorption_restart_time(config[CONF_ABSORPTION_RESTART_TIME]))

        if config.get(CONF_ABSORPTION_LOW_VOLTAGE_DELAY) is not None:
            cg.add(var.set_absorption_low_voltage_delay_s_(config[CONF_ABSORPTION_LOW_VOLTAGE_DELAY]))

    ###
    if config.get(CONF_EQUAIZATION_VOLTAGE) is not None:
        cg.add(var.set_equaization_voltage(config[CONF_EQUAIZATION_VOLTAGE]))

        if config.get(CONF_EQUAIZATION_TIME) is not None:
            cg.add(var.set_equaization_time(config[CONF_EQUAIZATION_TIME]))

        if config.get(CONF_EQUAIZATION_INTERVAL) is not None:
            cg.add(var.set_equaization_interval(config[CONF_EQUAIZATION_INTERVAL]))

        if config.get(CONF_EQUAIZATION_TIMEOUT) is not None:
            cg.add(var.set_equaization_timeout(config[CONF_EQUAIZATION_TIMEOUT]))


    if config.get(CONF_TARGET_SENSOR_VOLTAGE) is not None and config[CONF_TARGET_SENSOR_VOLTAGE]:
        sensV = await sensor.new_sensor(config[CONF_TARGET_SENSOR_VOLTAGE])
        cg.add(var.set_voltage_target_sensor(sensV))
      
