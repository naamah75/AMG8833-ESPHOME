# Home Assistant test setup

These credential-free examples reproduce the virtual-temperature control and dashboard used to test the firmware. Tested with Home Assistant 2026.10.4, Virtual Components 0.9.5 and ESPHome 2026.10.0.

## Virtual temperature

1. Install [Virtual Components](https://github.com/twrecked/hass-virtual) and restart HA if required by its installation instructions.
2. Copy `virtual_thermal_reference.yaml` into the HA configuration directory.
3. Add a Virtual Components integration group named `thermal_test_reference`, pointing to `/config/virtual_thermal_reference.yaml`. If editing an existing group, reload it after changing its file. Do not overwrite another group's file.
4. Merge the list item from `automation.yaml` into your existing `automations.yaml`, preserving other automations, then reload automations.
5. The group creates `sensor.thermal_virtual_reference_temperature` and `number.thermal_virtual_reference_setpoint`. Change the number to drive the sensor through the automation. Verify the actual entity IDs, especially if the names already exist.
6. Merge `esphome-reference.yaml` into the firmware configuration, then compile/upload. A valid configured local temperature sensor retains priority over this HA fallback.

The initial simulated value is **22 °C**. This tests the compensation control loop; it is not a real independent temperature measurement. To stop simulating, replace the imported entity with a real sensor or remove the HA reference configuration.

## Dashboard

`dashboard-view.yaml` is a single Sections view with the thermal camera, presence estimates, scene-learning button, reference control, compensation enable switch, automatic/manual/total offsets, background temperature, compensation status and frame diagnostics.

Merge it as one item under `views:` in a dashboard's raw configuration, or merge its `sections:` into an existing Sections view. Back up your dashboard first; do not replace unrelated views. Names are in Italian to match the test dashboard. Adjust entity IDs to your device names.

The temperature loop is continuous; there is no one-shot calibration button in this dashboard. The separate empty-scene button initializes presence detection. Before learning, the status explicitly reports that presence is uninitialized, so presence-based freezing is not yet available.

## Expected checks

- Changing the virtual number updates the virtual sensor and the ESPHome Reference Temperature entity.
- With automatic compensation enabled, Automatic Temperature Offset follows the reference-minus-scene-median target over time (30-second time constant).
- Manual trim is independent; Total Temperature Offset is the bounded sum used by corrected pixels.
- Disabling compensation removes only the automatic contribution.
- An unavailable reference holds the last automatic offset and is reported by the status entity.

The median assumes a majority background. Aligning surface readings with air temperature is scene normalization, not certified absolute radiometric calibration.
