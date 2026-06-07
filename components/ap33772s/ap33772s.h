#pragma once
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"
#include <vector>

namespace esphome {
namespace ap33772s {

struct FixedPdo {
  uint8_t index;   // 1-basierter PDO-Slot (1..13)
  uint16_t mv;     // Spannung in mV
};

class AP33772SOutput : public output::FloatOutput,
                       public Component,
                       public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(float state) override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  std::vector<FixedPdo> pdos_;
  int current_index_{-1};
  uint8_t retries_{0};

  void try_read_pdos_();
  bool read_pdos_();
  void request_index_(uint8_t pdo_index);
};

}  // namespace ap33772s
}  // namespace esphome
