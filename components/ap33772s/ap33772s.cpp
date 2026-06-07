#include "ap33772s.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>

namespace esphome {
namespace ap33772s {

static const char *const TAG = "ap33772s";

// AP33772S Registermap (DS46176), I2C-Adresse 0x52
static const uint8_t REG_STATUS = 0x01;  // 1 Byte Status
static const uint8_t REG_SRCPDO = 0x20;  // 26 Byte: 13 PDOs a 2 Byte (LE)
static const uint8_t REG_REQMSG = 0x31;  // 2 Byte: PD-Anforderung

void AP33772SOutput::setup() {
  ESP_LOGCONFIG(TAG, "Initialisiere AP33772S ...");
  this->try_read_pdos_();
}

void AP33772SOutput::try_read_pdos_() {
  if (this->read_pdos_()) {
    this->request_index_(this->pdos_.front().index);
    return;
  }
  if (this->retries_++ < 20) {
    this->set_timeout("pdo_retry", 500, [this]() { this->try_read_pdos_(); });
  } else {
    ESP_LOGE(TAG, "Keine Fixed-PDOs gefunden - PD-Quelle/Verhandlung pruefen!");
  }
}

bool AP33772SOutput::read_pdos_() {
  // --- DIAGNOSE: STATUS lesen ---
  uint8_t status = 0xAB;
  auto st_err = this->read_register(REG_STATUS, &status, 1);
  if (this->retries_ < 6)
    ESP_LOGI(TAG, "[try %u] STATUS read err=%d val=0x%02X", this->retries_, (int) st_err, status);

  // --- SRCPDO lesen (26 Byte) ---
  uint8_t buf[26] = {};
  auto err = this->read_register(REG_SRCPDO, buf, sizeof(buf));
  if (this->retries_ < 6) {
    ESP_LOGI(TAG, "[try %u] SRCPDO read err=%d", this->retries_, (int) err);
    ESP_LOGI(TAG, "[try %u] raw: %s", this->retries_,
             format_hex_pretty(buf, sizeof(buf)).c_str());
  }
  if (err != i2c::ERROR_OK)
    return false;

  this->pdos_.clear();
  for (uint8_t slot = 0; slot < 13; slot++) {
    uint16_t raw = (uint16_t) buf[slot * 2] | ((uint16_t) buf[slot * 2 + 1] << 8);
    if (raw != 0 && this->retries_ < 6)
      ESP_LOGI(TAG, "[try %u] Slot %u raw=0x%04X (detect=%u apdo=%u vmax=%u)",
               this->retries_, slot + 1, raw, (raw >> 15) & 1, (raw >> 14) & 1, raw & 0xFF);

    bool detect = raw & 0x8000;
    bool is_apdo = raw & 0x4000;
    if (!detect || is_apdo)
      continue;
    uint16_t vmax = raw & 0x00FF;
    uint16_t mv = (slot < 7) ? (uint16_t)(vmax * 100) : (uint16_t)(vmax * 200);
    if (mv == 0)
      continue;
    this->pdos_.push_back(FixedPdo{(uint8_t)(slot + 1), mv});
  }

  std::sort(this->pdos_.begin(), this->pdos_.end(),
            [](const FixedPdo &a, const FixedPdo &b) { return a.mv < b.mv; });

  if (this->pdos_.empty())
    return false;

  for (auto &p : this->pdos_)
    ESP_LOGI(TAG, "Fixed-PDO Slot %u: %u mV", p.index, p.mv);
  return true;
}

void AP33772SOutput::write_state(float state) {
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
    ESP_LOGE(TAG, "PD_REQMSG schreiben fehlgeschlagen");
  }
}

void AP33772SOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "AP33772S USB-PD Output:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Gefundene Fixed-PDOs: %u", (unsigned) this->pdos_.size());
}

}  // namespace ap33772s
}  // namespace esphome
