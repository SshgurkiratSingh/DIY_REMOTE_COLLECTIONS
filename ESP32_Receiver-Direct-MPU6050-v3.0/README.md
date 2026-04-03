# ESP32 Receiver - Direct MPU6050 v3.0

ESP32 receiver firmware for a differential-drive robot using:
- ESP-NOW input commands
- Dual H-bridge motor control (left/right)
- MPU6050-based heading/tilt estimation
- 3 drive behaviors selected via transmitter toggles

## Features

- Mode 1: Direct Mapping (default)
- Mode 2: Headless / Absolute Direction (Toggle 2)
- Mode 3: Full Assist (Toggle 1)
  - Heading hold
  - Anti-tip (pitch/roll safety)
  - Smooth acceleration ramping
  - Hill-hold assist on incline
  - IMU-based traction limiting
- Push Button 2: Set Home Direction (stores current yaw baseline for Mode 2)
- Command timeout safety (stops motors if packets stop)

## Project Structure

- src/main.cpp: ESP-NOW receive path, scheduler loop, component orchestration
- src/MotorController.cpp: PWM motor output and ramp application
- src/MpuEstimator.cpp: MPU6050 reading, yaw integration, pitch/roll estimation
- src/DriveModes.cpp: drive mode logic and assist features
- include/ControlTypes.h: shared packet types and state structs
- include/MotorController.h: motor component interface
- include/MpuEstimator.h: IMU component interface
- include/DriveModes.h: mode logic interface
- platformio.ini: board/env config and library dependencies

## Control Mapping

Incoming ESP-NOW payload (`struct_message`) fields:
- `joyX`: steering input
- `joyY`: throttle input
- `potValue`: speed/ramp scaling
- `toggle1`: Full Assist enable (Mode 3)
- `toggle2`: Headless enable (Mode 2)
- `push1`: burst mode
- `push2`: set home direction (rising edge)

Mode behavior:
- Toggle1 OFF + Toggle2 OFF => Mode 1 Direct Mapping
- Toggle2 ON => Mode 2 Headless mapping active
- Toggle1 ON => Mode 3 assist active
- Toggle1 ON + Toggle2 ON => combined mode (headless first, then assist corrections)

## Hardware Pins (Current Firmware)

Motor pin assignments in src/main.cpp:

Left motor:
- RPWM: GPIO18
- LPWM: GPIO19
- LEN: GPIO21
- REN: GPIO22

Right motor:
- RPWM: GPIO25
- LPWM: GPIO26
- LEN: GPIO27
- REN: GPIO14

MPU6050 I2C:
- SDA: GPIO32
- SCL: GPIO33

Note: default ESP32 I2C pins 21/22 are not used here because those are assigned to motor enable pins.

## Build Environment

Defined in platformio.ini:
- Board: `esp32dev`
- Framework: `arduino`
- Monitor baud: `115200`
- Libraries:
  - `adafruit/Adafruit MPU6050`
  - `adafruit/Adafruit Unified Sensor`

## Build and Flash

From project root:

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

## Runtime Notes

- Yaw is derived from gyro-Z integration (no magnetometer), so slow drift over time is expected.
- Use Push2 periodically to re-zero the home heading used by Mode 2.
- If no valid command packet is received within the timeout window, the firmware commands neutral/stop.

## Tuning Parameters

Primary mode tuning constants live in src/DriveModes.cpp:
- `ANTI_TIP_LIMIT_DEG`
- `HILL_HOLD_PITCH_DEG`
- `HILL_HOLD_GAIN`
- `HEADING_HOLD_GAIN`
- traction response thresholds and scale limits

Motor ramp behavior is controlled by:
- `MIN_RAMP_RATE` and `MAX_RAMP_RATE` (DriveModes)
- ramp application logic in src/MotorController.cpp

## Compatibility Note

The transmitter and receiver must use the exact same packet layout for `struct_message`.
If you change fields/order/types in include/ControlTypes.h, update transmitter firmware accordingly.
