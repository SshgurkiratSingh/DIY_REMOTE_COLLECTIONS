# ESP-NOW OLED v1.2

## Purpose
Current main handheld transmitter with optimized runtime behavior.

## Key Features
- OLED redraw only on content change.
- EMA filtering on analog channels.
- Hysteresis-based deadzone logic.
- Persistent settings and captive portal target management.

## Hardware Required
- ESP32 development board
- SSD1306 OLED (I2C)
- Joystick, potentiometer, switches, push buttons, rotary encoder

## Build/Upload
```bash
cd firmware/transmitter/esp-now-oled/v1.2
pio run
pio run --target upload
```

## Compatible Pairings
- `firmware/receiver/direct-bts/v2.0`
- `firmware/receiver/direct-bts-mpu6050/v3.0`

## Changelog
See `CHANGELOG.md` in this folder.
