#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/alloc_helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <array>
#include "presence.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

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
  void set_calibrated_pixels(text_sensor::TextSensor *s) { calibrated_pixels_ = s; }
  void set_presence(binary_sensor::BinarySensor *s) { presence_ = s; }
  void set_presence_ready(binary_sensor::BinarySensor *s) { presence_ready_ = s; }
  void set_people_count(sensor::Sensor *s) { people_count_ = s; }
  void set_reference_temperature(sensor::Sensor *s) { reference_temperature_ = s; }
  void set_reference_source(text_sensor::TextSensor *s) { reference_source_ = s; }
  void set_local_temperature_sensor(sensor::Sensor *s) { local_reference_ = s; }
  void set_homeassistant_temperature_sensor(sensor::Sensor *s) { ha_reference_ = s; }
  void set_presence_delta(float v) { estimator_.settings.delta = v; }
  void set_presence_minimum_temperature(float v) { estimator_.settings.minimum_temperature = v; }
  void set_presence_maximum_temperature(float v) { estimator_.settings.maximum_temperature = v; }
  void set_presence_minimum_pixels(uint8_t v) { estimator_.settings.minimum_pixels = v; }
  void set_presence_maximum_pixels(uint8_t v) { estimator_.settings.maximum_pixels = v; }
  void set_presence_confirmation(uint32_t v) { estimator_.settings.confirmation_ms = v; }
  void set_presence_release(uint32_t v) { estimator_.settings.release_ms = v; }
  void set_temperature_offset(float v) { if (std::isfinite(v) && std::abs(v) <= 15) offset_ = v; }
  void learn_empty_scene() {
    estimator_.request_empty_scene();
    ESP_LOGI(TAG, "Empty scene learning requested; keep the field of view empty");
  }
  float calculate_temperature_offset() {
    if (!have_frame_ || millis() - last_frame_ms_ > 3000) {
      ESP_LOGW(TAG, "Calibration refused: no recent frame");
      return NAN;
    }
    const float value = calibration_offset(last_frame_values_, get_reference_());
    if (!std::isfinite(value)) ESP_LOGW(TAG, "Calibration refused: missing reference, nonuniform scene or excessive offset");
    return value;
  }
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
      if (!read_bytes(0x80 + offset, raw.data() + offset, 16)) { frame_error_(); return; }
    }
    if (!read_bytes(0x0E, therm, 2)) { frame_error_(); return; }
    float lo = 1000, hi = -1000, sum = 0;
    std::array<uint8_t, 64> legacy{};
    for (size_t i = 0; i < 64; i++) {
      uint16_t value = raw[2*i] | (uint16_t(raw[2*i+1]) << 8);
      float t = (value & 0x7FF) * 0.25f;
      if (value & 0x800) t = -t;
      last_frame_values_[i] = t;
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
    have_frame_ = true;
    last_frame_ms_ = millis();
    const auto result = estimator_.process(last_frame_values_, last_frame_ms_, offset_);
    if (presence_ready_ != nullptr) presence_ready_->publish_state(result.ready);
    if (result.ready) {
      if (presence_ != nullptr) presence_->publish_state(result.count > 0);
      if (people_count_ != nullptr) people_count_->publish_state(result.count);
    } else {
      if (presence_ != nullptr) presence_->invalidate_state();
      if (people_count_ != nullptr) people_count_->publish_state(NAN);
    }
    const float local = sensor_value_(local_reference_);
    const float remote = sensor_value_(ha_reference_);
    const float reference = select_reference(local, remote);
    if (reference_temperature_ != nullptr) reference_temperature_->publish_state(reference);
    if (reference_source_ != nullptr) reference_source_->publish_state(
        valid_reference(local) ? "Local sensor" : (valid_reference(remote) ? "Home Assistant" : "Unavailable"));
    if (calibrated_pixels_ != nullptr) {
      std::array<uint8_t, 128> corrected{};
      for (size_t i = 0; i < 64; ++i) {
        const float value = std::max(-511.75f, std::min(511.75f, last_frame_values_[i] + offset_));
        uint16_t encoded = static_cast<uint16_t>(std::round(std::abs(value) * 4));
        if (value < 0) encoded |= 0x800;
        corrected[i*2] = encoded & 0xFF;
        corrected[i*2+1] = encoded >> 8;
      }
      calibrated_pixels_->publish_state(base64_encode(corrected.data(), corrected.size()));
    }
    status_clear_warning();
  }
 protected:
  static float sensor_value_(sensor::Sensor *s) {
    return s != nullptr && s->has_state() ? s->state : NAN;
  }
  float get_reference_() { return select_reference(sensor_value_(local_reference_), sensor_value_(ha_reference_)); }
  void frame_error_() {
    status_set_warning();
    have_frame_ = false;
    if (presence_ != nullptr) presence_->invalidate_state();
    if (people_count_ != nullptr) people_count_->publish_state(NAN);
    if (presence_ready_ != nullptr) presence_ready_->publish_state(false);
    if (calibrated_pixels_ != nullptr) calibrated_pixels_->invalidate_state();
  }
  PresenceEstimator estimator_;
  std::array<float, 64> last_frame_values_{};
  uint32_t last_frame_ms_{0};
  bool have_frame_{false};
  float offset_{0};
  sensor::Sensor *local_reference_{nullptr}, *ha_reference_{nullptr};
  sensor::Sensor *people_count_{nullptr}, *reference_temperature_{nullptr};
  binary_sensor::BinarySensor *presence_{nullptr}, *presence_ready_{nullptr};
  text_sensor::TextSensor *calibrated_pixels_{nullptr}, *reference_source_{nullptr};
  sensor::Sensor *temperature_, *minimum_, *maximum_, *average_;
  text_sensor::TextSensor *pixels_;
  text_sensor::TextSensor *raw_pixels_{nullptr};
};
}}
