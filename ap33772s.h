#pragma once
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ap33772s {

class AP33772SOutput : public output::FloatOutput,
                       public Component,
                       public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(float state) override;

 protected:
  uint8_t num_pdos_{0};
  uint16_t voltages_mv_[7]{};
  int current_pdo_{-1};
  void request_pdo_(int pdo_index);
};

}  // namespace ap33772s
}  // namespace esphome
