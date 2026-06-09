#pragma once
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"
#include <vector>

namespace esphome {
namespace ap33772s {

struct FixedPdo {
  uint8_t index;
  uint16_t mv;
};

class AP33772SOutput : public output::FloatOutput,
                       public Component,
                       public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(float state) override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Konfiguration (alles in mV)
  void set_max_mv(uint16_t v) { this->max_mv_ = v; }
  void set_min_mv(uint16_t v) { this->min_mv_ = v; }
  void set_prefer_pps(bool b) { this->prefer_pps_ = b; }

  // Messwerte
  uint16_t get_measured_mv() const { return this->measured_mv_; }   // VOUT in mV
  uint16_t get_requested_mv() const { return this->requested_mv_; } // ausgehandelt in mV
  uint16_t get_current_ma() const { return this->current_ma_; }     // IOUT in mA
  uint32_t get_power_mw() const {
    return (uint32_t) this->measured_mv_ * this->current_ma_ / 1000;
  }

 protected:
  uint16_t max_mv_{15000};
  uint16_t min_mv_{5000};
  bool prefer_pps_{true};

  std::vector<FixedPdo> pdos_;
  bool use_pps_{false};
  uint8_t pps_index_{0};
  uint16_t pps_max_mv_{0};

  int current_index_{-1};
  uint16_t target_mv_{0};
  uint8_t retries_{0};
  uint16_t measured_mv_{0};
  uint16_t requested_mv_{0};
  uint16_t current_ma_{0};

  void try_read_pdos_();
  bool read_pdos_();
  void request_index_(uint8_t pdo_index);
  void request_pps_(uint16_t mv);
  void read_power_();
  uint16_t pps_eff_max_() const;
};

}  // namespace ap33772s
}  // namespace esphome
