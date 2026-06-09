#include "ap33772s.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>

namespace esphome {
namespace ap33772s {

static const char *const TAG = "ap33772s";

static const uint8_t REG_VOLTAGE = 0x11;  // 2 Byte VOUT, LSB 80 mV
static const uint8_t REG_CURRENT = 0x12;  // 1 Byte IOUT, LSB 24 mA
static const uint8_t REG_VREQ    = 0x14;  // 2 Byte ausgehandelt, LSB 50 mV
static const uint8_t REG_SRCPDO  = 0x20;  // 26 Byte, 13 PDOs a 2 Byte (LE)
static const uint8_t REG_REQMSG  = 0x31;  // 2 Byte PD-Anforderung

void AP33772SOutput::setup() {
  ESP_LOGCONFIG(TAG, "Initialisiere AP33772S ...");
  this->try_read_pdos_();
  this->set_interval("power", 3000, [this]() { this->read_power_(); });
  this->set_interval("pps_keepalive", 500, [this]() {
    if (this->use_pps_ && this->target_mv_ > 0)
      this->request_pps_(this->target_mv_);
  });
}

uint16_t AP33772SOutput::pps_eff_max_() const {
  return std::min(this->pps_max_mv_, this->max_mv_);
}

void AP33772SOutput::try_read_pdos_() {
  if (this->read_pdos_()) {
    if (this->use_pps_) {
      this->target_mv_ = this->min_mv_;
      this->request_pps_(this->min_mv_);
    } else {
      this->request_index_(this->pdos_.front().index);
    }
    return;
  }
  if (this->retries_++ < 20) {
    this->set_timeout("pdo_retry", 500, [this]() { this->try_read_pdos_(); });
  } else {
    ESP_LOGE(TAG, "Keine nutzbaren PDOs gefunden - PD-Quelle/Verhandlung pruefen!");
  }
}

void AP33772SOutput::read_power_() {
  uint8_t v[2] = {};
  if (this->read_register(REG_VOLTAGE, v, 2) == i2c::ERROR_OK) {
    uint16_t raw = (uint16_t) v[0] | ((uint16_t) v[1] << 8);
    this->measured_mv_ = (uint16_t)(raw * 80);
  }
  uint8_t c = 0;
  if (this->read_register(REG_CURRENT, &c, 1) == i2c::ERROR_OK) {
    this->current_ma_ = (uint16_t)(c * 24);
  }
  uint8_t r[2] = {};
  if (this->read_register(REG_VREQ, r, 2) == i2c::ERROR_OK) {
    uint16_t raw = (uint16_t) r[0] | ((uint16_t) r[1] << 8);
    this->requested_mv_ = (uint16_t)(raw * 50);
  }
  ESP_LOGI(TAG, "VOUT=%u mV  I=%u mA  P=%u mW  (VREQ=%u mV)",
           this->measured_mv_, this->current_ma_, (unsigned) this->get_power_mw(),
           this->requested_mv_);
}

bool AP33772SOutput::read_pdos_() {
  uint8_t buf[26] = {};
  if (this->read_register(REG_SRCPDO, buf, sizeof(buf)) != i2c::ERROR_OK)
    return false;

  this->pdos_.clear();
  uint8_t best_pps_index = 0;
  uint16_t best_pps_max = 0;

  for (uint8_t slot = 0; slot < 13; slot++) {
    uint16_t raw = (uint16_t) buf[slot * 2] | ((uint16_t) buf[slot * 2 + 1] << 8);
    bool detect = raw & 0x8000;
    if (!detect)
      continue;
    bool is_apdo = raw & 0x4000;
    uint16_t vmax_units = raw & 0x00FF;

    if (!is_apdo) {
      uint16_t mv = (slot < 7) ? (uint16_t)(vmax_units * 100) : (uint16_t)(vmax_units * 200);
      if (mv == 0)
        continue;
      ESP_LOGI(TAG, "Fixed-PDO Slot %u: %u mV%s", slot + 1, mv,
               (mv > this->max_mv_) ? " (uebersprungen, > max)" : "");
      if (mv <= this->max_mv_)
        this->pdos_.push_back(FixedPdo{(uint8_t)(slot + 1), mv});
    } else if (slot < 7) {
      uint16_t apdo_max = (uint16_t)(vmax_units * 100);
      ESP_LOGI(TAG, "PPS-APDO Slot %u: bis %u mV", slot + 1, apdo_max);
      if (apdo_max > best_pps_max) {
        best_pps_max = apdo_max;
        best_pps_index = (uint8_t)(slot + 1);
      }
    }
  }

  std::sort(this->pdos_.begin(), this->pdos_.end(),
            [](const FixedPdo &a, const FixedPdo &b) { return a.mv < b.mv; });

  this->use_pps_ = false;
  if (this->prefer_pps_ && best_pps_max >= (uint16_t)(this->min_mv_ + 1000)) {
    this->use_pps_ = true;
    this->pps_index_ = best_pps_index;
    this->pps_max_mv_ = best_pps_max;
  }

  if (this->use_pps_) {
    ESP_LOGI(TAG, "Modus: PPS (Slot %u), Regelbereich %u..%u mV in 100-mV-Schritten",
             this->pps_index_, this->min_mv_, this->pps_eff_max_());
    return true;
  }
  if (!this->pdos_.empty()) {
    ESP_LOGI(TAG, "Modus: feste Stufen (auf %u mV begrenzt), %u Stufen",
             this->max_mv_, (unsigned) this->pdos_.size());
    return true;
  }
  return false;
}

void AP33772SOutput::write_state(float state) {
  if (this->use_pps_) {
    uint16_t eff_max = this->pps_eff_max_();
    uint16_t target;
    if (state <= 0.0f) {
      target = this->min_mv_;
    } else {
      float span = (float) (eff_max - this->min_mv_);
      target = (uint16_t)(this->min_mv_ + state * span);
    }
    target = (uint16_t)((target / 100) * 100);
    if (target < this->min_mv_) target = this->min_mv_;
    if (target > eff_max) target = eff_max;

    if (target != this->target_mv_) {
      this->target_mv_ = target;
      this->request_pps_(target);
      ESP_LOGI(TAG, "PPS-Ziel: %u mV (state=%.2f)", target, state);
    }
    return;
  }

  if (this->pdos_.empty())
    return;
  int n = (int) this->pdos_.size();
  int idx;
  if (state <= 0.0f) {
    idx = 0;
  } else {
    idx = (int) (state * (float) n);
    if (idx >= n)
      idx = n - 1;
  }
  uint8_t pdo_index = this->pdos_[idx].index;
  if ((int) pdo_index != this->current_index_)
    this->request_index_(pdo_index);
}

void AP33772SOutput::request_index_(uint8_t pdo_index) {
  uint16_t msg = ((uint16_t) pdo_index << 12) | 0x0FFF;
  uint8_t data[2] = {(uint8_t)(msg & 0xFF), (uint8_t)(msg >> 8)};
  if (this->write_register(REG_REQMSG, data, 2) == i2c::ERROR_OK) {
    this->current_index_ = pdo_index;
    uint16_t mv = 0;
    for (auto &p : this->pdos_)
      if (p.index == pdo_index)
        mv = p.mv;
    ESP_LOGI(TAG, "Angefordert: PDO %u -> %u mV", pdo_index, mv);
  } else {
    ESP_LOGE(TAG, "PD_REQMSG (fix) schreiben fehlgeschlagen");
  }
}

void AP33772SOutput::request_pps_(uint16_t mv) {
  uint8_t vsel = (uint8_t)(mv / 100);
  uint16_t msg = ((uint16_t) this->pps_index_ << 12) | (0xF << 8) | vsel;
  uint8_t data[2] = {(uint8_t)(msg & 0xFF), (uint8_t)(msg >> 8)};
  this->write_register(REG_REQMSG, data, 2);
}

void AP33772SOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "AP33772S USB-PD Output:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Max-Spannung: %u mV", this->max_mv_);
  ESP_LOGCONFIG(TAG, "  Min-Spannung: %u mV", this->min_mv_);
  ESP_LOGCONFIG(TAG, "  PPS bevorzugt: %s", this->prefer_pps_ ? "ja" : "nein");
}

}  // namespace ap33772s
}  // namespace esphome
