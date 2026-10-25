#include "components/amg8833/presence.h"
#include <cassert>
#include <iostream>
#include <limits>

using esphome::amg8833::PresenceEstimator;
using esphome::amg8833::calibration_offset;
using esphome::amg8833::select_reference;

std::array<float, 64> scene(float temperature = 20) {
  std::array<float, 64> values{};
  values.fill(temperature);
  return values;
}
void blob(std::array<float, 64> &values, size_t origin, float temperature = 30) {
  values[origin] = values[origin + 1] = values[origin + 8] = temperature;
}
PresenceEstimator ready_model(const std::array<float, 64> &empty = scene()) {
  PresenceEstimator model;
  assert(!model.process(empty, 0).ready);
  model.request_empty_scene();
  auto r = model.process(empty, 0);
  assert(r.valid && r.ready && r.count == 0);
  return model;
}

int main() {
  const auto empty = scene();
  {
    auto model = ready_model();
    for (uint32_t t = 1000; t <= 10000; t += 1000) assert(model.process(empty, t).count == 0);
  }
  {
    auto model = ready_model();
    auto person = scene(); blob(person, 9);
    assert(model.process(person, 1000).count == 0);
    assert(model.process(person, 2000).count == 0);
    assert(model.process(person, 3000).count == 1);
    // A stationary target must never be absorbed by the adaptive background.
    for (uint32_t t = 4000; t <= 600000; t += 1000) assert(model.process(person, t).count == 1);
    for (uint32_t t = 601000; t <= 620000; t += 1000) model.process(empty, t);
    assert(model.process(empty, 621000).count == 0);
  }
  {
    auto model = ready_model();
    auto two = scene(); blob(two, 9); blob(two, 45);
    for (uint32_t t = 1000; t <= 4000; t += 1000) model.process(two, t);
    assert(model.process(two, 5000).count == 2);
  }
  {
    auto model = ready_model();
    auto noisy = scene(); noisy[20] = 30;
    for (uint32_t t = 1000; t <= 10000; t += 1000) assert(model.process(noisy, t).count == 0);
  }
  {
    auto heater = scene(); blob(heater, 9, 55);
    auto model = ready_model(heater);
    for (uint32_t t = 1000; t <= 10000; t += 1000) assert(model.process(heater, t).count == 0);
  }
  {
    auto model = ready_model();
    auto transient = scene(); blob(transient, 9);
    model.process(transient, 1000);
    for (uint32_t t = 2000; t <= 10000; t += 1000) assert(model.process(empty, t).count == 0);
  }
  {
    auto model = ready_model();
    auto target = scene(); blob(target, 9);
    model.process(target, 1000);
    // Missing samples do not count as continuous confirmation.
    assert(model.process(target, 10000).count == 0);
    assert(model.process(target, 11000).count == 0);
    assert(model.process(target, 12000).count == 1);
    auto invalid = target; invalid[0] = NAN;
    assert(!model.process(invalid, 13000).valid);
  }
  {
    auto model = ready_model();
    const auto warmer_room = scene(25);
    for (uint32_t t = 1000; t <= 10000; t += 1000) assert(model.process(warmer_room, t).count == 0);
    auto target = warmer_room; blob(target, 9, 34);
    for (uint32_t t = 11000; t <= 15000; t += 1000) model.process(target, t);
    assert(model.process(target, 16000).count == 1);
  }
  {
    // Learning explicitly clears the old estimate; no implicit warm-up learning.
    auto model = ready_model(); auto target = scene(); blob(target, 9);
    for (uint32_t t = 1000; t <= 4000; t += 1000) model.process(target, t);
    model.request_empty_scene();
    assert(model.process(empty, 5000).count == 0);
  }
  assert(select_reference(21, 22) == 21);
  assert(select_reference(NAN, 22) == 22);
  assert(select_reference(100, 22) == 22);
  assert(std::isnan(select_reference(NAN, NAN)));
  assert(calibration_offset(scene(20), 22) == 2);
  assert(calibration_offset(scene(22), 20) == -2);
  assert(std::isnan(calibration_offset(scene(20), NAN)));
  assert(std::isnan(calibration_offset(scene(20), 50)));
  auto uneven = scene(); uneven[0] = 30;
  assert(std::isnan(calibration_offset(uneven, 22)));
  std::cout << "PASS: warm targets, stationary presence, separated count, noise, static heater, "
               "timing, invalid frames, relearning, reference priority and calibration guards\n";
}
