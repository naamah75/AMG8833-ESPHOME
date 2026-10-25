# Changelog

## 2026.10.1-beta.1

- Add explicit empty-scene learning, stationary warm-target presence estimates and connected-region counting with confirmation/release delays.
- Compensate common scene temperature drift and reject tiny components.
- Add optional local temperature reference with Home Assistant fallback.
- Add guarded one-point calibration, persistent manual offset and corrected pixel frames without changing raw measurements.
- Add native tests of the production C++ algorithm and two validated reference configuration examples.
- Validate by local/GitHub builds, live OTA, full-frame offset checks and HA camera rendering. Human-count accuracy in a real installation is not yet established.

## 2026.10.0

- Migrate custom components to an external component and ESPHome I2C APIs.
- Update ESP8266 board and OTA configuration for ESPHome 2026.10.0.
- Publish a complete 128-byte raw pixel frame alongside legacy compatibility data.
- Preserve negative temperatures and temperatures above 63.75 Â°C in raw frames.
- Publish thermistor, minimum, maximum and average temperatures from one acquisition.
- Use a template contrast sensor; the former min/max index entities are not included.
- Add credential-free example secrets and a pinned compilation workflow.

Verified on D1 Mini Pro and Grove AMG8833 with HA 2026.10.2 and the companion Thermal fork.
