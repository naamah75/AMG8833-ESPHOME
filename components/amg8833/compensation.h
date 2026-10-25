#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace esphome { namespace amg8833 {

// Continuous scene normalization, not an absolute radiometric calibration.
class SceneCompensation {
 public:
  float time_constant_seconds{30};
  float maximum_offset{10};
  bool enabled{true};
  struct Result {
    float automatic_offset;
    float total_offset;
    float background;
    const char *status;
  };
  Result update(const std::array<float, 64> &frame, float reference, float manual,
                bool foreground, bool presence_ready, uint32_t now) {
    const float elapsed = initialized_ ? std::min((now - last_) * 0.001f, 5.0f) : 1.0f;
    initialized_ = true;
    last_ = now;
    auto sorted = frame;
    bool valid = true;
    for (float v : sorted) valid = valid && std::isfinite(v);
    float background = NAN;
    if (valid) {
      std::sort(sorted.begin(), sorted.end());
      background = (sorted[31] + sorted[32]) * 0.5f;
    }
    const char *status;
    if (!enabled) status = "Disabled";
    else if (!valid) status = "Frame unavailable (holding)";
    else if (!std::isfinite(reference) || reference < -20 || reference > 60)
      status = "Reference unavailable (holding)";
    else if (foreground) status = "Presence detected (holding)";
    else {
      const float desired = reference - background;
      const float target = std::max(-maximum_offset, std::min(maximum_offset, desired));
      const float alpha = 1.0f - std::exp(-elapsed / time_constant_seconds);
      automatic_ += alpha * (target - automatic_);
      status = std::abs(desired) > maximum_offset ? "Offset limited" :
          (presence_ready ? "Tracking" : "Tracking (presence uninitialized)");
    }
    const float automatic = enabled ? automatic_ : 0;
    const float total = std::max(-15.0f, std::min(15.0f, automatic + manual));
    return {automatic, total, background, status};
  }
 protected:
  float automatic_{0};
  bool initialized_{false};
  uint32_t last_{0};
};
}}
