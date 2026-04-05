# ESP-NOW OLED Remote Controller - v1.3.0

A high-performance ESP32-based wireless remote controller using the ESP-NOW protocol with an OLED display, analog joystick, multiple buttons, and a configurable Haptic/Audio Feedback system.

## What's New in v1.3.0

### 1️⃣ **Haptic & Audio Feedback System**
- Repurposed the LED pins (Pins 18 & 23) to support a Vibrator Motor and Buzzer.
- Introduced a non-blocking `FeedbackManager` to safely trigger audio/haptic patterns without halting the main control loop.
- **Continuous Haptic Feedback**: The vibrator now stays continuously active for the exact duration the push buttons are held down.

### 2️⃣ **Connection State Alerts**
- The `NetworkManager` now tracks ESP-NOW packet delivery success rates.
- Triggers distinct tactile and audible alerts upon connection loss (after consecutive failed TX packets) and connection recovery.

### 3️⃣ **Dynamic OLED Menu Scrolling**
- Upgraded the `DisplayManager` to handle menus exceeding the display's rows.
- Implemented a sliding window indexer to smoothly scroll through settings menus with more than 8 items.

### 4️⃣ **New Configuration Options**
- **Output Type**: Toggle the function of the output pins between legacy `LEDs` or the new `Vibrator/Buzzer` modes.
- **Feedback Mode**: Select your preferred feedback mix: `Silent`, `Vibrator Only`, `Buzzer Only`, or `Both`.
- **Test Haptic**: A new settings menu utility to safely trigger and test the connection loss feedback sequence.

---

## Features

### Hardware Support

- **Analog Inputs (ADC1)**
  - Joystick X-axis (Pin 35)
  - Joystick Y-axis (Pin 33)
  - Potentiometer (Pin 32)
- **Digital Inputs**
  - Rotary Encoder CLK (Pin 27)
  - Rotary Encoder DT (Pin 14)
  - Encoder Switch (Pin 13)
  - Toggle 1 & 2 (Pins 26, 25)
  - Push Buttons 1 & 2 (Pins 19, 15)
- **Outputs**
  - SSD1306 OLED Display 128x64 (I2C: SDA=21, SCL=22)
  - Virbrator Motor / LED Yellow (Pin 18)
  - Buzzer / LED Green (Pin 23)

### Display Modes

| State | Description |
| --- | --- |
| **HUD** | Real-time joystick visualization with TX/RX status |
| **TELEMETRY** | Detailed sensor readings (raw ADC values) |
| **RX VIEWER** | Displays received data from the remote device |
| **TARGET SELECT** | Choose which MAC address to transmit to |
| **SETTINGS** | Configure deadzone, inversion, TX rate, outputs, and feedback |
| **EDIT PARAM** | Adjust individual settings with the rotary encoder |
| **ABOUT** | Firmware version and MAC address |

## Quick Start

### 1. Build & Upload

```bash
cd firmware/transmitter/esp-now-oled/v1.3
platformio run -e esp32dev --target upload --target monitor
```

### 2. Initial Configuration
- Power on the device.
- Navigate to **SETTINGS** → **EDIT PARAM** using the encoder.
- Modify the **Output Type** to match your hardware (LEDs vs Vib/Buzzer).
- Adjust your **Feedback Mode**.
- Press the encoder once to save.

## Development

### Project Structure

```
esp-now-oled/v1.3/
├── include/
│   ├── Config.h               # Hardware pins & structures
│   ├── DisplayManager.h       # OLED display controller & UI
│   ├── FeedbackManager.h      # Haptic & Audio sequencing
│   ├── InputManager.h         # ADC & button input with filtering
│   ├── NetworkManager.h       # ESP-NOW communication
│   ├── PortalManager.h        # Web configuration portal
│   └── StorageManager.h       # EEPROM/Preferences storage
├── lib/
├── src/
│   ├── main.cpp               # Main FSM and event loop
│   ├── DisplayManager.cpp
│   ├── FeedbackManager.cpp
│   ├── InputManager.cpp
│   ├── NetworkManager.cpp
│   ├── PortalManager.cpp
│   └── StorageManager.cpp
├── platformio.ini             # Build configuration
└── CHANGELOG.md               # Version history
```

### Key Managers

- **FeedbackManager (New)**: Uses multi-step sequence polling against `millis()` and dynamic hardware `ledc` PWM toggling to play complex audio and haptic feedback profiles without blocking execution.
- **DisplayManager**: Employs a hash-based change detection system to minimize OLED I2C bandwidth wear, and now supports scalable sliding-window scrolling for nested UI menus.
- **NetworkManager**: Handles ESP-NOW transmission/reception and actively monitors link saturation to trigger connection alarms.
