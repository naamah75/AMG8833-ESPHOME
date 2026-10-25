#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace esphome { namespace amg8833 {

// A thermal-blob estimator, not a human classifier. Kept independent of ESPHome
// so the production algorithm can be exercised by native C++ regression tests.
class PresenceEstimator {
 public:
  struct Settings {
    float delta{2.5f};
    float minimum_temperature{20.0f};
    float maximum_temperature{40.0f};
    uint8_t minimum_pixels{3};
    uint8_t maximum_pixels{32};
    uint32_t confirmation_ms{2000};
    uint32_t release_ms{10000};
    float background_tau_seconds{300.0f};
  };
  struct Result {
    bool valid{false};
    bool ready{false};
    uint8_t count{0};
    uint8_t instantaneous_count{0};
  };

  Settings settings;

  void request_empty_scene() { learn_requested_ = true; }
  bool ready() const { return ready_; }

  Result process(const std::array<float, 64> &frame, uint32_t now, float offset = 0) {
    for (float value : frame) {
      if (!std::isfinite(value)) return {};
    }
    if (learn_requested_) {
      background_ = frame;
      smooth_ = frame;
      ready_ = true;
      learn_requested_ = false;
      initialized_ = true;
      count_ = candidate_ = 0;
      candidate_since_ = now;
      last_frame_ = now;
      return {true, true, 0, 0};
    }
    if (!ready_) return {true, false, 0, 0};
    const uint32_t elapsed = initialized_ ? now - last_frame_ : 1000;
    if (elapsed > 5000) candidate_since_ = now;
    last_frame_ = now;
    initialized_ = true;
    // Time-based filtering behaves the same at 1 Hz and faster sample rates.
    const float dt = std::min(elapsed * 0.001f, 5.0f);
    const float alpha = 1.0f - std::exp(-dt / 0.7f);
    std::array<bool, 64> foreground{};
    for (size_t i = 0; i < 64; ++i) {
      smooth_[i] += alpha * (frame[i] - smooth_[i]);
      const float corrected = smooth_[i] + offset;
      foreground[i] = smooth_[i] - background_[i] >= settings.delta &&
          corrected >= settings.minimum_temperature && corrected <= settings.maximum_temperature;
    }
    std::array<bool, 64> visited{};
    uint8_t detected = 0;
    for (size_t origin = 0; origin < 64; ++origin) {
      if (!foreground[origin] || visited[origin]) continue;
      std::array<uint8_t, 64> queue{};
      size_t head = 0, tail = 0;
      queue[tail++] = static_cast<uint8_t>(origin);
      visited[origin] = true;
      while (head < tail) {
        const int pixel = queue[head++];
        const int row = pixel / 8, col = pixel % 8;
        for (int dr = -1; dr <= 1; ++dr) {
          for (int dc = -1; dc <= 1; ++dc) {
            const int r = row + dr, c = col + dc;
            if (r < 0 || r >= 8 || c < 0 || c >= 8) continue;
            const size_t neighbor = r * 8 + c;
            if (foreground[neighbor] && !visited[neighbor]) {
              visited[neighbor] = true;
              queue[tail++] = static_cast<uint8_t>(neighbor);
            }
          }
        }
      }
      if (tail >= settings.minimum_pixels && tail <= settings.maximum_pixels) ++detected;
    }
    if (detected != candidate_) {
      candidate_ = detected;
      candidate_since_ = now;
    }
    const uint32_t delay = candidate_ < count_ ? settings.release_ms : settings.confirmation_ms;
    if (candidate_ != count_ && now - candidate_since_ >= delay) count_ = candidate_;
    // Freeze the entire scene while any warm foreground exists (including tiny
    // or rejected blobs), so a stationary target is never learned as background.
    const bool any_foreground = std::any_of(foreground.begin(), foreground.end(), [](bool v) { return v; });
    if (!any_foreground && count_ == 0) {
      const float beta = 1.0f - std::exp(-dt / settings.background_tau_seconds);
      for (size_t i = 0; i < 64; ++i) background_[i] += beta * (smooth_[i] - background_[i]);
    }
    return {true, true, count_, detected};
  }

 protected:
  std::array<float, 64> background_{};
  std::array<float, 64> smooth_{};
  bool ready_{false};
  bool initialized_{false};
  bool learn_requested_{false};
  uint8_t count_{0};
  uint8_t candidate_{0};
  uint32_t candidate_since_{0};
  uint32_t last_frame_{0};
};

// Air temperature is only a valid offset reference when the scene is an
// isothermal target equilibrated with that reference. Caller invokes explicitly.
inline float calibration_offset(const std::array<float, 64> &frame, float reference,
                                float maximum_spread = 1.5f) {
  if (!std::isfinite(reference) || reference < -20 || reference > 60) return NAN;
  auto sorted = frame;
  for (float value : sorted) if (!std::isfinite(value)) return NAN;
  std::sort(sorted.begin(), sorted.end());
  if (sorted.back() - sorted.front() > maximum_spread) return NAN;
  const float offset = reference - (sorted[31] + sorted[32]) * 0.5f;
  return std::abs(offset) <= 15.0f ? offset : NAN;
}

inline bool valid_reference(float value) {
  return std::isfinite(value) && value >= -20 && value <= 60;
}

inline float select_reference(float local, float home_assistant) {
  return valid_reference(local) ? local : (valid_reference(home_assistant) ? home_assistant : NAN);
}

}}
