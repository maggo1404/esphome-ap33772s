import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, output
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@maggo1404"]

ap33772s_ns = cg.esphome_ns.namespace("ap33772s")
AP33772SOutput = ap33772s_ns.class_(
    "AP33772SOutput", output.FloatOutput, cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(AP33772SOutput),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x52))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    await i2c.register_i2c_device(var, config)
