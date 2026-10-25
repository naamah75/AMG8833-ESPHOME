#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/alloc_helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <array>

namespace esphome { namespace amg8833 {
static const char *const TAG = "amg8833";
class AMG8833 : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature(sensor::Sensor *s) { temperature_ = s; }
  void set_minimum(sensor::Sensor *s) { minimum_ = s; }
  void set_maximum(sensor::Sensor *s) { maximum_ = s; }
  void set_average(sensor::Sensor *s) { average_ = s; }
  void set_pixels(text_sensor::TextSensor *s) { pixels_ = s; }
  void set_raw_pixels(text_sensor::TextSensor *s) { raw_pixels_ = s; }
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override {
    if (!write_byte(0x00, 0x00) || !write_byte(0x01, 0x3F)) { mark_failed(); return; }
    set_timeout(100, [this]() {
      if (!write_byte(0x02, 0x00) || !write_byte(0x03, 0x00)) mark_failed();
    });
  }
  void dump_config() override { ESP_LOGCONFIG("amg8833", "AMG8833 thermal array"); LOG_I2C_DEVICE(this); }
  void update() override {
    std::array<uint8_t, 128> raw{};
    uint8_t therm[2];
    // Small transactions also work with the ESP8266 Wire buffer limit.
    for (size_t offset = 0; offset < raw.size(); offset += 16) {
      if (!read_bytes(0x80 + offset, raw.data() + offset, 16)) { status_set_warning(); return; }
    }
    if (!read_bytes(0x0E, therm, 2)) { status_set_warning(); return; }
    float lo = 1000, hi = -1000, sum = 0;
    std::array<uint8_t, 64> legacy{};
    for (size_t i = 0; i < 64; i++) {
      uint16_t value = raw[2*i] | (uint16_t(raw[2*i+1]) << 8);
      float t = (value & 0x7FF) * 0.25f;
      if (value & 0x800) t = -t;
      lo = std::min(lo, t); hi = std::max(hi, t); sum += t;
      // Thermal Vision v3.1.7 pixel_sensor uses unsigned bytes: 0..63.75 C.
      legacy[i] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, t * 4)));
    }
    uint16_t v = therm[0] | (uint16_t(therm[1]) << 8);
    float t = (v & 0x7FF) * 0.0625f;
    temperature_->publish_state((v & 0x800) ? -t : t);
    minimum_->publish_state(lo); maximum_->publish_state(hi); average_->publish_state(sum/64);
    pixels_->publish_state(base64_encode(legacy.data(), legacy.size()));
    if (raw_pixels_ != nullptr) raw_pixels_->publish_state(base64_encode(raw.data(), raw.size()));
    status_clear_warning();
  }
 protected:
  sensor::Sensor *temperature_, *minimum_, *maximum_, *average_;
  text_sensor::TextSensor *pixels_;
  text_sensor::TextSensor *raw_pixels_{nullptr};
};
}}
