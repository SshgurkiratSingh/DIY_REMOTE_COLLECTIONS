# ESP-NOW OLED v1.1

## Purpose
Mature transmitter revision with stable menu and settings flow.

## Hardware Required
- ESP32 development board
- SSD1306 OLED (I2C)
- Joystick, switches, buttons, rotary encoder

## Build/Upload
```bash
cd firmware/transmitter/esp-now-oled/v1.1
pio run
pio run --target upload
```

## Compatible Pairings
- `firmware/receiver/direct-bts/v1.0`
- `firmware/receiver/esp-now-legacy/v1.1.2`

## Known Notes
Pre-v1.2 input smoothing and deadzone behavior.
