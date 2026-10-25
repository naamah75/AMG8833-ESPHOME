import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor"]
ns = cg.esphome_ns.namespace("amg8833")
AMG8833 = ns.class_("AMG8833", cg.PollingComponent, i2c.I2CDevice)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(AMG8833),
    cv.Required("pixels"): text_sensor.text_sensor_schema(),
    cv.Optional("raw_pixels"): text_sensor.text_sensor_schema(),
    cv.Required("temperature"): sensor.sensor_schema(unit_of_measurement="°C", accuracy_decimals=2, device_class="temperature", state_class="measurement"),
    cv.Required("minimum"): sensor.sensor_schema(unit_of_measurement="°C", accuracy_decimals=2, device_class="temperature", state_class="measurement"),
    cv.Required("maximum"): sensor.sensor_schema(unit_of_measurement="°C", accuracy_decimals=2, device_class="temperature", state_class="measurement"),
    cv.Required("average"): sensor.sensor_schema(unit_of_measurement="°C", accuracy_decimals=2, device_class="temperature", state_class="measurement"),
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
