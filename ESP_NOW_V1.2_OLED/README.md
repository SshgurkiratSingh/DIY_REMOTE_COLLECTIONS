# ESP-NOW OLED Remote Controller - v1.2.0

A high-performance ESP32-based wireless remote controller using ESP-NOW protocol with OLED display, analog joystick, multiple buttons, and configurable LED indicators.

## What's New in v1.2.0

### Three Major Improvements

#### 1️⃣ **OLED Display Optimization**

- Display only refreshes when content changes (not every frame)
- Reduces OLED wear significantly
- Lowers I2C bandwidth usage
- LED indicators update in real-time every cycle

#### 2️⃣ **ADC Noise Filtering**

- Exponential Moving Average (EMA) filter on all analog channels
- Eliminates the ~10 unit jitter on ESP32 ADC inputs
- Smooth, stable joystick readings
- Fully tunable smoothing factor

#### 3️⃣ **Hysteresis-Based Deadzone**

- Prevents center jitter and drift
- Only exits deadzone with meaningful movement (deadzone + 8 units margin)
- Smooth, precise control with zero vibration

**See [CHANGELOG.md](CHANGELOG.md) for technical details.**

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
  - LED Yellow (Pin 23)
  - LED Green (Pin 18)

### Display Modes

| State                   | Description                                               |
| ----------------------- | --------------------------------------------------------- |
| **HUD**           | Real-time joystick visualization with TX/RX status        |
| **TELEMETRY**     | Detailed sensor readings (raw ADC values)                 |
| **RX VIEWER**     | Displays received data from remote device                 |
| **TARGET SELECT** | Choose which MAC address to transmit to                   |
| **SETTINGS**      | Configure deadzone, inversion, TX rate, LED mode, RX mode |
| **EDIT PARAM**    | Adjust individual settings with encoder                   |
| **ABOUT**         | Firmware version and MAC address                          |

### LED Modes

| Mode                 | Green LED     | Yellow LED | Behavior                           |
| -------------------- | ------------- | ---------- | ---------------------------------- |
| **TX_RX**      | TX Status     | TX Status  | Green=OK, Yellow=Fail              |
| **LINK_STATE** | Link OK       | Link Fail  | Shows connection state             |
| **ACTION**     | Button Active | Off        | Green when buttons/toggles pressed |
| **STEALTH**    | Off           | Off        | LEDs always off                    |

### Configuration Options

- **Deadzone**: 0-2000 units (default: 300)
- **Inversion**: Toggle X and Y axes independently
- **TX Rate**: 5-250 Hz
- **RX Mode**: Enable/disable receiving telemetry
- **LED Mode**: Select indicator behavior

## Quick Start

### 1. Build & Upload

```bash
cd /home/gurkirat/Projects/REMOTE_ESPNOW_LORA_RF_BLE/ESP_NOW_V1.2_OLED
platformio run -e esp32dev --target upload --target monitor
```

### 2. Initial Calibration

- Power on the device
- Navigate to **SETTINGS** → **EDIT PARAM** using encoder
- Adjust **Deadzone** to your preference (typically 200-400 units)
- Press encoder once to save

### 3. Add Target Device

- Go to **SETTINGS** → **Add Tgt(AP)**
- Connect to WiFi SSID: `ESP-NOW-REMOTE`
- Password: `12345678`
- Navigate to `192.168.4.1`
- Enter target device MAC address
- Device will reboot and store the target

## Tuning Guide

### Adjusting ADC Smoothing (EMA Filter)

**File**: [src/InputManager.cpp](src/InputManager.cpp)

```cpp
const float InputManager::EMA_ALPHA = 0.15f;  // Default: 15% new + 85% old
```

| Value | Effect                                    |
| ----- | ----------------------------------------- |
| 0.05  | Very smooth, slower response              |
| 0.10  | Balanced, smooth                          |
| 0.15  | Default - smooth with good responsiveness |
| 0.20  | Faster, more jittery                      |
| 0.30+ | Too responsive, visible noise             |

**Recommendation**: Start with default (0.15) and adjust if joystick feels sluggish or noisy.

### Adjusting Deadzone Hysteresis

**File**: [include/InputManager.h](include/InputManager.h)

```cpp
static const uint16_t HYSTERESIS_MARGIN = 8;  // Default: 8 units margin
```

| Value | Effect                                                     |
| ----- | ---------------------------------------------------------- |
| 4     | Tighter deadzone, more sensitive                           |
| 8     | Default - balanced                                         |
| 12    | Larger margin, more stable but less responsive at boundary |

**Recommendation**: Keep default unless experiencing boundary jitter.

## Performance Metrics

### Memory Usage (v1.2.0)

- **RAM**: 14.3% (46,984 / 327,680 bytes)
- **Flash**: 66.0% (865,693 / 1,310,720 bytes)

### Display Refresh Rate

- **Before**: 50-60 updates/sec (continuous redraw)
- **After**: ~5-10 updates/sec (change-based)
- **Improvement**: 5-10x reduction in OLED traffic

## Development

### Project Structure

```
ESP_NOW_V1.2_OLED/
├── include/
│   ├── Config.h              # Hardware pins & structures
│   ├── DisplayManager.h       # OLED display controller
│   ├── InputManager.h         # ADC & button input with filtering
│   ├── NetworkManager.h       # ESP-NOW communication
│   ├── PortalManager.h        # Web configuration portal
│   └── StorageManager.h       # NVS persistent storage
├── src/
│   ├── main.cpp              # Main FSM and event loop
│   ├── DisplayManager.cpp
│   ├── InputManager.cpp
│   ├── NetworkManager.cpp
│   ├── PortalManager.cpp
│   └── StorageManager.cpp
├── platformio.ini            # Build configuration
└── CHANGELOG.md              # Version history
```

### Key Classes

#### DisplayManager

- Hash-based change detection to minimize OLED updates
- Support for 7 different display states
- Integrated LED control

#### InputManager

- EMA filtering for smooth ADC readings
- Hysteresis-based deadzone
- Encoder position tracking

#### NetworkManager

- ESP-NOW transmission/reception
- Target MAC management

#### StorageManager

- NVS persistent storage
- Configuration backup/restore

#### PortalManager

- WiFi AP for web configuration
- Target device addition
