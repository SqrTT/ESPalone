from esphome import automation
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.core import ID
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE

from .. import template_ns

def toIDs(sensor):
    return f"{sensor.id}"

ReactiveTemplateSensor = template_ns.class_(
    "ReactiveTemplateSensor", sensor.Sensor, cg.Component
)
CONF_SENSORS = "depends_on_sensors"


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        ReactiveTemplateSensor,
        accuracy_decimals=1,
    )
    .extend(
        {
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            # array of sensor_schema
            cv.Optional(CONF_SENSORS): cv.ensure_list(cv.use_id(cg.EntityBase)),
        }
    )
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(float)
        )
        cg.add(var.set_template(template_))

    dependsOnSensors = []

    if CONF_SENSORS in config:
        dependsOnSensors = config[CONF_SENSORS]
    elif CONF_LAMBDA in config:
        dependsOnSensors = config[CONF_LAMBDA].requires_ids

    # expr = cg.RawExpression(f"static sensor::Sensor * const {dependsOnName}[{len(dependsOnSensors)}] = " + "{ " + ",".join(map(toIDs,dependsOnSensors)) + " }")
    for s in dependsOnSensors:
        sens = await cg.get_variable(s)
        cg.add(var.add_to_track(sens))

    
    # cg.add(var.set_depends_on_sensors(cg.RawExpression(f"{dependsOnName}"), len(dependsOnSensors)));


@automation.register_action(
    "sensor.template.publish",
    sensor.SensorPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
            cv.Required(CONF_STATE): cv.templatable(cv.float_),
        }
    ),
)
async def sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, float)
    cg.add(var.set_state(template_))
    return var