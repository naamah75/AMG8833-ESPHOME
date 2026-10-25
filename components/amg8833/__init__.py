import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, i2c, sensor, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
ns = cg.esphome_ns.namespace("amg8833")
AMG8833 = ns.class_("AMG8833", cg.PollingComponent, i2c.I2CDevice)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(AMG8833),
    cv.Required("pixels"): text_sensor.text_sensor_schema(),
    cv.Optional("raw_pixels"): text_sensor.text_sensor_schema(),
    cv.Optional("calibrated_pixels"): text_sensor.text_sensor_schema(),
    cv.Optional("local_temperature_sensor"): cv.use_id(sensor.Sensor),
    cv.Optional("homeassistant_temperature_sensor"): cv.use_id(sensor.Sensor),
    cv.Optional("reference_temperature"): sensor.sensor_schema(unit_of_measurement="°C", accuracy_decimals=2, device_class="temperature"),
    cv.Optional("reference_source"): text_sensor.text_sensor_schema(entity_category="diagnostic"),
    cv.Optional("presence"): cv.Schema({
        cv.Required("occupancy"): binary_sensor.binary_sensor_schema(device_class="occupancy"),
        cv.Required("people_count"): sensor.sensor_schema(accuracy_decimals=0, state_class="measurement"),
        cv.Required("ready"): binary_sensor.binary_sensor_schema(entity_category="diagnostic"),
        cv.Optional("delta", default=2.5): cv.float_range(min=0.5, max=15),
        cv.Optional("minimum_temperature", default=20): cv.float_range(min=-20, max=60),
        cv.Optional("maximum_temperature", default=40): cv.float_range(min=-20, max=80),
        cv.Optional("minimum_pixels", default=3): cv.int_range(min=1, max=64),
        cv.Optional("maximum_pixels", default=32): cv.int_range(min=1, max=64),
        cv.Optional("confirmation", default="2s"): cv.positive_time_period_milliseconds,
        cv.Optional("release", default="10s"): cv.positive_time_period_milliseconds,
    }),
    cv.Required("temperature"): sensor.sensor_schema(unit_of_measurement="Â°C", accuracy_decimals=2, device_class="temperature", state_class="measurement"),
    cv.Required("minimum"): sensor.sensor_schema(unit_of_measurement="Â°C", accuracy_decimals=2, device_class="temperature", state_class="measurement"),
    cv.Required("maximum"): sensor.sensor_schema(unit_of_measurement="Â°C", accuracy_decimals=2, device_class="temperature", state_class="measurement"),
    cv.Required("average"): sensor.sensor_schema(unit_of_measurement="Â°C", accuracy_decimals=2, device_class="temperature", state_class="measurement"),
}).extend(cv.polling_component_schema("1s")).extend(i2c.i2c_device_schema(0x69))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    for key in ("temperature", "minimum", "maximum", "average"):
        entity = await sensor.new_sensor(config[key])
        cg.add(getattr(var, "set_" + key)(entity))
    entity = await text_sensor.new_text_sensor(config["pixels"])
    cg.add(var.set_pixels(entity))
    if "raw_pixels" in config:
        entity = await text_sensor.new_text_sensor(config["raw_pixels"])
        cg.add(var.set_raw_pixels(entity))

    if "calibrated_pixels" in config:
        entity = await text_sensor.new_text_sensor(config["calibrated_pixels"])
        cg.add(var.set_calibrated_pixels(entity))
    for key in ("local_temperature_sensor", "homeassistant_temperature_sensor"):
        if key in config:
            cg.add(getattr(var, "set_" + key)(await cg.get_variable(config[key])))
    if "reference_temperature" in config:
        cg.add(var.set_reference_temperature(await sensor.new_sensor(config["reference_temperature"])))
    if "reference_source" in config:
        cg.add(var.set_reference_source(await text_sensor.new_text_sensor(config["reference_source"])))
    if "presence" in config:
        pres = config["presence"]
        if pres["minimum_temperature"] >= pres["maximum_temperature"]:
            raise cv.Invalid("Presence minimum_temperature must be below maximum_temperature")
        if pres["minimum_pixels"] > pres["maximum_pixels"]:
            raise cv.Invalid("Presence minimum_pixels must not exceed maximum_pixels")
        cg.add(var.set_presence(await binary_sensor.new_binary_sensor(pres["occupancy"])))
        cg.add(var.set_people_count(await sensor.new_sensor(pres["people_count"])))
        cg.add(var.set_presence_ready(await binary_sensor.new_binary_sensor(pres["ready"])))
        for key in ("delta", "minimum_temperature", "maximum_temperature", "minimum_pixels", "maximum_pixels"):
            cg.add(getattr(var, "set_presence_" + key)(pres[key]))
        cg.add(var.set_presence_confirmation(pres["confirmation"].total_milliseconds))
        cg.add(var.set_presence_release(pres["release"].total_milliseconds))
