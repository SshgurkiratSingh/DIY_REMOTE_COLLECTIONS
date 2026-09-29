# REMOTE_ESPNOW_LORA_RF_BLE

An ESP32-based RC remote control collection centred around an ESP-NOW handheld transmitter with an OLED UI, rotary-encoder navigation, persistent target storage, captive-portal pairing, and a full telemetry reply path. All variants are standalone PlatformIO projects.

---

## Firmware Overview

### Transmitter

| Version | Path | Notes |
|---------|------|-------|
| **v2.0** ⭐ | `firmware/transmitter/esp-now-oled/v2.0` | Current production version. Packed `struct_message`, `addrLedMode` for addressable LED control, pairing keys, haptic/buzzer feedback, 11-item settings menu. |
| v1.3 | `firmware/transmitter/esp-now-oled/v1.3` | Pre-release v2.0 |
| v1.2 | `firmware/transmitter/esp-now-oled/v1.2` | EMA filter, captive portal, NVS persistence |
| v1.1 | `firmware/transmitter/esp-now-oled/v1.1` | Older OLED iteration |
| v1.0 | `firmware/transmitter/esp-now-oled/v1.0` | Initial OLED transmitter |

### Receivers — `direct-l298n` (ESP-NOW, L298N)

| Version | Path | Notes |
|---------|------|-------|
| **v2.0-led** ⭐ | `firmware/receiver/direct-l298n/v2.0-led` | **Current production receiver.** L298N motor driver + dual WS2812B eye-matrix LED strips (10 LEDs each, 3-3-2 layout). 10 custom eye animations. Full v2.0 transmitter payload. |

### Receivers — `direct-bts` (ESP-NOW, BTS7960)

| Version | Path | Notes |
|---------|------|-------|
| v3.0 | `firmware/receiver/direct-bts/v3.0` | Extended receiver with pairing code (`verifyKey`) |
| v2.1 | `firmware/receiver/direct-bts/v2.1` | Dual motor, reply support |
| v2.0 | `firmware/receiver/direct-bts/v2.0` | Base dual-motor receiver |
| v1.0 | `firmware/receiver/direct-bts/v1.0` | Motor test stub (no radio) |

### Receivers — `direct-bts-mpu6050` (ESP-NOW + IMU)

| Version | Path | Notes |
|---------|------|-------|
| v3.0 | `firmware/receiver/direct-bts-mpu6050/v3.0` | MPU6050 estimator, mode-aware drive control (headless / full assist) |

### Receivers — `fsi6-bts` (FlySky FS-i6 IBUS protocol)

| Version | Path | Notes |
|---------|------|-------|
| v1.1-mpu-l2n | `firmware/receiver/fsi6-bts/v1.1-mpu-l2n` | FS-i6 + MPU6050 + L298N |
| v1.1-mpu | `firmware/receiver/fsi6-bts/v1.1-mpu` | FS-i6 + MPU6050 + BTS7960 |
| v1.0-l2n | `firmware/receiver/fsi6-bts/v1.0-l2n` | FS-i6 + L298N |
| v1.0 | `firmware/receiver/fsi6-bts/v1.0` | FS-i6 baseline |

### Receivers — `esp-now-legacy` (older payload format)

| Version | Path | Notes |
|---------|------|-------|
| v1.1.2 | `firmware/receiver/esp-now-legacy/v1.1.2` | Sends active reply, 7-field payload |
| v1.1 | `firmware/receiver/esp-now-legacy/v1.1` | Includes `encoderPos` / `encSw` fields |

> ⚠️ Legacy receivers use a **different, older packet format** and are not compatible with the v2.0 transmitter.

---

## Packet Format (v2.0 Protocol)

All receivers under `direct-bts` (v2.0, v2.1, v3.0), `direct-l298n` (v2.0-led), and `direct-bts-mpu6050` now use the same packed payload:

### Transmitter → Receiver

```cpp
typedef struct __attribute__((packed)) struct_message {
    uint16_t joyX;        // Joystick X axis  (0–4095, center ≈ 2048)
    uint16_t joyY;        // Joystick Y axis  (0–4095, center ≈ 2048)
    uint16_t potValue;    // Potentiometer    (0–4095)  → LED brightness
    bool     toggle1;     // Toggle switch 1  → motor-LED sync
    bool     toggle2;     // Toggle switch 2  → motor enable (safety kill)
    bool     push1;       // Push button 1
    bool     push2;       // Push button 2
    uint8_t  verifyKey;   // Pairing key (0 = accept all)
    uint8_t  addrLedMode; // Addressable LED mode (0–9), set from Settings menu
} struct_message;  // 12 bytes
```

### Receiver → Transmitter (telemetry reply)

```cpp
typedef struct __attribute__((packed)) rx_message {
    uint8_t data1;   // Active LED mode (0–9)
    uint8_t data2;   // Motor-sync flag (0 or 1)
    uint8_t data3;   // Battery level   (0–255)
} rx_message;  // 3 bytes
```

---

## Feature Highlights — v2.0 Transmitter

- SSD1306 128×64 OLED with hash-based dirty-redraw (no flicker)
- Rotary encoder menu navigation (scroll + click + double-click)
- **11-item Settings menu:** Deadzone · Invert X/Y · TX Rate · RX Mode · LED Map · Out Type · Feedback · Haptic Test · **Addr LED Mode**
- NVS persistence via `Preferences` (all settings survive reboot)
- Captive-portal Wi-Fi AP for adding receiver MACs without re-flashing
- Up to 5 saved receiver targets with last-target recall
- Optional haptic/buzzer feedback on button events
- Sends `addrLedMode` live to receiver every TX packet

---

## Feature Highlights — v2.0-led Receiver (Eye Matrix)

- **Dual WS2812B LED strips** — left eye / right eye, 10 LEDs each
- **Physical layout per eye:** Line 1 (LEDs 0-2) · hidden routing LED (3) · Line 2 (LEDs 4-6) · hidden routing LED (7) · Line 3 (LEDs 8-9)
- Hidden LEDs (index 3 and 7) are always masked off in software
- **10 eye animation modes** selectable from remote settings:

| # | Name | Description |
|---|------|-------------|
| 0 | Solid Headlights | Crisp white on all visible LEDs |
| 1 | Angry Eyes | Top + middle lines red, bottom off |
| 2 | Scanning Pupil | Cylon-style column sweep left↔right |
| 3 | Natural Blinking | White with periodic ~150 ms blink |
| 4 | Sleepy Breathing | Cyan sine-pulse on lower two lines only |
| 5 | Rainbow Flow | Hue shifts across the eye row-by-row |
| 6 | Hypnotic Lines | Cycles top→mid→bottom with colour change |
| 7 | Fire Flicker | Organic random warm flame |
| 8 | Police Strobe | Left eye red, right eye blue, alternating |
| 9 | Cyber Sparkle | Random-hue digital twinkling |

- **Motor-LED sync mode** (Toggle 1): LEDs reflect driving state
  - Idle → gentle green breathing
  - Forward → green sparkle sweep
  - Reverse → red sparkle sweep  
  - Turning → orange blink on turn side
  - Full speed → rapid rainbow burst
  - Braking → red flash
- **Motor enable/disable** (Toggle 2 — safety kill switch)
- **Live brightness** from potentiometer (floor at 10 so strip never goes dark)
- **Signal-loss indicator:** slow red pulse when no packet received > 1 s

---

## Pin Mapping

### Transmitter v2.0

| Function | GPIO |
|---|--:|
| Joystick Y | 33 |
| Potentiometer | 32 |
| Joystick X | 35 |
| Encoder CLK | 27 |
| Encoder DT | 14 |
| Encoder SW | 13 |
| Toggle 1 | 26 |
| Toggle 2 | 25 |
| Push 1 | 19 |
| Push 2 | 15 |
| OLED SDA | 21 |
| OLED SCL | 22 |
| Yellow LED | 23 |
| Green LED | 18 |

### Receiver v2.0-led (L298N + Dual Eye LED)

| Function | GPIO |
|---|--:|
| Motor A ENA (PWM) | 25 |
| Motor A IN1 | 26 |
| Motor A IN2 | 27 |
| Motor B ENB (PWM) | 14 |
| Motor B IN3 | 12 |
| Motor B IN4 | 13 |
| Left Eye LED Data | 15 |
| Right Eye LED Data | 16 |
| Status LED (onboard) | 2 |
| Battery ADC (optional) | 34 |

---

## Repository Layout

```text
.
├── firmware/
│   ├── transmitter/
│   │   └── esp-now-oled/
│   │       ├── v1.0/
│   │       ├── v1.1/
│   │       ├── v1.2/
│   │       ├── v1.3/
│   │       └── v2.0/          ← current transmitter
│   └── receiver/
│       ├── direct-l298n/
│       │   └── v2.0-led/      ← current receiver (LED eye matrix)
│       ├── direct-bts/
│       │   ├── v1.0/
│       │   ├── v2.0/
│       │   ├── v2.1/
│       │   └── v3.0/
│       ├── direct-bts-mpu6050/
│       │   └── v3.0/
│       ├── fsi6-bts/
│       │   ├── v1.0/
│       │   ├── v1.0-l2n/
│       │   ├── v1.1-mpu/
│       │   └── v1.1-mpu-l2n/
│       └── esp-now-legacy/
│           ├── v1.1/
│           └── v1.1.2/
├── bridges/
│   └── arduino-bts/
└── .github/workflows/
```

---

## Quick Start

### 1. Flash the receiver

```bash
cd firmware/receiver/direct-l298n/v2.0-led
pio run --target upload
pio device monitor        # note the printed MAC address
```

### 2. Flash the transmitter

```bash
cd firmware/transmitter/esp-now-oled/v2.0
pio run --target upload
```

### 3. Pair

1. On the transmitter OLED, navigate to **Settings → Add Tgt(AP)**
2. Connect to Wi-Fi: SSID `ESP-NOW-REMOTE` / password `12345678`
3. Open `http://192.168.4.1` and enter the receiver MAC + a name
4. Save — the transmitter reboots and loads the new target automatically

### 4. Control LED modes

In the transmitter Settings menu, scroll to **Addr LED** (item 10) and turn the encoder to select a mode (0–9). The receiver updates instantly.

---

## Building Any Variant

Each folder is an independent PlatformIO project:

```bash
cd <firmware-folder>
pio run                   # compile
pio run --target upload   # compile + flash
pio device monitor        # serial monitor @ 115200
```
