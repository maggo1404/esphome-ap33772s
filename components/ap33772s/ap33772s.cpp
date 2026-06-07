#include "ap33772s.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ap33772s {

static const char *const TAG = "ap33772s";

// ⚠️ Registeradressen bitte gegen das AP33772S-Datenblatt prüfen!
static const uint8_t REG_PDOINFO     = 0x00;  // Bit[2:0] = Anzahl Source-PDOs
static const uint8_t REG_SRCPDO_BASE = 0x01;  // PDO 1..7, je 4 Bytes (LE)
static const uint8_t REG_REQPDO      = 0x08;  // Schreibe PDO-Index (1-basiert)

void AP33772SOutput::setup() {
  ESP_LOGCONFIG(TAG, "Initialisiere AP33772S ...");

  uint8_t info = 0;
  if (this->read_register(REG_PDOINFO, &info, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "I2C-Fehler - Verkabelung und Adresse pruefen!");
    this->mark_failed();
    return;
  }

  this->num_pdos_ = info & 0x07;
  ESP_LOGI(TAG, "%d Source-PDOs gefunden", this->num_pdos_);

  for (uint8_t i = 0; i < this->num_pdos_ && i < 7; i++) {
    uint8_t data[4] = {};
    this->read_register(REG_SRCPDO_BASE + i, data, 4);
    uint32_t pdo = (uint32_t) data[0] | ((uint32_t) data[1] << 8) |
                   ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
    // Fixed-Supply-PDO: Spannung in Bits[19:10], Einheit 50 mV
    this->voltages_mv_[i] = ((pdo >> 10) & 0x3FF) * 50;
    ESP_LOGI(TAG, "  PDO %d: %u mV", i + 1, this->voltages_mv_[i]);
  }

  this->request_pdo_(1);  // Beim Start Mindestspannung
}

void AP33772SOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "AP33772S USB-PD Output:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Verfuegbare PDOs: %d", this->num_pdos_);
}

void AP33772SOutput::write_state(float state) {
  if (this->is_failed() || this->num_pdos_ == 0)
    return;

  int idx;
  if (state <= 0.0f) {
    idx = 0;  // PDO 1 = kleinste Spannung (haelt den ESP32 am Leben)
  } else {
    idx = (int) (state * (float) this->num_pdos_);
    if (idx >= this->num_pdos_)
      idx = this->num_pdos_ - 1;
  }

  if (idx + 1 != this->current_pdo_)
    this->request_pdo_(idx + 1);
}

void AP33772SOutput::request_pdo_(int pdo_index) {
  uint8_t val = (uint8_t) pdo_index;
  if (this->write_register(REG_REQPDO, &val, 1) == i2c::ERROR_OK) {
    this->current_pdo_ = pdo_index;
    uint16_t mv = (pdo_index <= this->num_pdos_) ? this->voltages_mv_[pdo_index - 1] : 0;
    ESP_LOGI(TAG, "PDO %d angefordert -> %u mV", pdo_index, mv);
  } else {
    ESP_LOGE(TAG, "Fehler beim Schreiben von REG_REQPDO");
  }
}

}  // namespace ap33772s
}  // namespace esphome
