import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUS_VOLTAGE,
    CONF_CURRENT,
    CONF_ID,
    CONF_MAX_CURRENT,
    CONF_MAX_VOLTAGE,
    CONF_POWER,
    CONF_SHUNT_RESISTANCE,
    CONF_SHUNT_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_HERTZ,
    ENTITY_CATEGORY_DIAGNOSTIC
)
from ..coulomb_meter import (COULOMB_SCHEMA, setup_coulomb, CoulombMeter_ns)
AUTO_LOAD = ["coulomb_meter"]
DEPENDENCIES = ["i2c"]
CONF_READ_PER_SECOND = "read_per_second"
UNIT_COULOMB = "C"
CONF_CHARGE_COULOMBS = "charge_coulombs"

ina219_ns = cg.esphome_ns.namespace("ina219_coulomb")
INA219Component = ina219_ns.class_(
    "INA219Component", CoulombMeter_ns, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(INA219Component),
            cv.Optional(CONF_BUS_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SHUNT_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SHUNT_RESISTANCE, default=0.1): cv.All(
                cv.resistance, cv.Range(min=0.0, max=32.0)
            ),
            cv.Optional(CONF_MAX_VOLTAGE, default=32.0): cv.All(
                cv.voltage, cv.Range(min=0.0, max=32.0)
            ),
            cv.Optional(CONF_MAX_CURRENT, default=3.2): cv.All(
                cv.current, cv.Range(min=0.0)
            ),
            cv.Optional(CONF_READ_PER_SECOND): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                accuracy_decimals=1,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_CHARGE_COULOMBS): sensor.sensor_schema(
                unit_of_measurement=UNIT_COULOMB,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x40))
    .extend(COULOMB_SCHEMA) 
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_shunt_resistance_ohm(config[CONF_SHUNT_RESISTANCE]))
    cg.add(var.set_max_current_a(config[CONF_MAX_CURRENT]))
    cg.add(var.set_max_voltage_v(config[CONF_MAX_VOLTAGE]))

    if CONF_BUS_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_BUS_VOLTAGE])
        cg.add(var.set_bus_voltage_sensor(sens))

    if CONF_SHUNT_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_SHUNT_VOLTAGE])
        cg.add(var.set_shunt_voltage_sensor(sens))

    if CONF_CURRENT in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT])
        cg.add(var.set_current_sensor(sens))

    if CONF_POWER in config:
        sens = await sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(sens))

    if conf := config.get(CONF_READ_PER_SECOND):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_read_per_second_sensor(sens))

    if conf := config.get(CONF_CHARGE_COULOMBS):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_coulombs_sensor(sens))

    await setup_coulomb(var, config)
