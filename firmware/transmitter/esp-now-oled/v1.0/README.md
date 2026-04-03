# ESP-NOW OLED v1.0

## Purpose
Baseline transmitter implementation used as historical reference.

## Hardware Required
- ESP32 development board
- SSD1306 OLED (I2C)
- Joystick, switches, buttons, rotary encoder

## Build/Upload
```bash
cd firmware/transmitter/esp-now-oled/v1.0
pio run
pio run --target upload
```

## Compatible Pairings
- `firmware/receiver/esp-now-legacy/v1.1`
- `firmware/receiver/esp-now-legacy/v1.1.2`

## Known Notes
No advanced filtering or display-change optimization in this revision.
