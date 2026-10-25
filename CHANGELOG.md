# Changelog

## 2026.10.0

- Migrate custom components to an external component and ESPHome I2C APIs.
- Update ESP8266 board and OTA configuration for ESPHome 2026.10.0.
- Publish a complete 128-byte raw pixel frame alongside legacy compatibility data.
- Preserve negative temperatures and temperatures above 63.75 °C in raw frames.
- Publish thermistor, minimum, maximum and average temperatures from one acquisition.
- Use a template contrast sensor; the former min/max index entities are not included.
- Add credential-free example secrets and a pinned compilation workflow.

Verified on D1 Mini Pro and Grove AMG8833 with HA 2026.10.2 and the companion Thermal fork.
