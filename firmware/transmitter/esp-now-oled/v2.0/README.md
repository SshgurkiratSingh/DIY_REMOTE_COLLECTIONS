# ESP-NOW OLED Remote Controller — v2.0

A high-performance ESP32-based wireless remote controller using the ESP-NOW protocol. Features a 128×64 SSD1306 OLED display, rotary-encoder menu navigation, captive-portal pairing, NVS persistence, haptic/audio feedback, and a **Settings menu item to select the addressable LED mode on the receiver**.

---

## What's New in v2.0

### 1️⃣ Addressable LED Mode Control
- New **Settings item 10: Addr LED** — use the rotary encoder to scroll through modes 0–9
- The selected mode is transmitted live in every ESP-NOW packet via the `addrLedMode` field
- The receiver (e.g. `direct-bts/v2.0-led`) switches eye animations instantly, no button press on the car required
- Setting is persisted to NVS and restored on reboot

### 2️⃣ Updated & Packed Protocol Struct
- `struct_message` now carries `verifyKey` + `addrLedMode` and is marked `__attribute__((packed))` to guarantee identical wire layout on any compiler
- All `direct-bts` receivers (v2.0, v2.1, v3.0, v2.0-led) and `direct-bts-mpu6050/v3.0` updated to match

### 3️⃣ Haptic & Audio Feedback System (from v1.3)
- Vibrator motor and buzzer on output pins 18 & 23
- Non-blocking `FeedbackManager` — complex patterns without stalling the control loop
- Continuous haptic while push buttons are held

### 4️⃣ Connection State Alerts
- `NetworkManager` tracks consecutive TX failures
- Distinct tactile + audible alerts on link loss and recovery

### 5️⃣ Scrollable Settings Menu (11 items)
- Sliding-window renderer supports more items than the display can show at once

---

## Settings Menu

| # | Item | Description |
|---|------|-------------|
| 0 | Add Tgt (AP) | Start captive portal to add a new receiver |
| 1 | Deadzone | ADC deadzone (0–2000) |
| 2 | Inv X/Y | Toggle axis inversion |
| 3 | Factory Rst | Wipe all stored settings and reboot |
| 4 | Tx Rate | Packets per second (5–250 Hz) |
| 5 | Rx Mode | Enable/disable telemetry receive |
| 6 | LED Maps | Transmitter status LED behaviour |
| 7 | Out Type | LEDs vs Vibrator/Buzzer output pins |
| 8 | Feedback | Silent / Vib / Buzz / Both |
| 9 | Test Haptic | Fire connection-loss pattern for testing |
| **10** | **Addr LED** | **Select receiver LED mode 0–9** |

---

## Display Screens

| Screen | Description |
|--------|-------------|
| HUD | Real-time joystick bars, button state, TX/RX status |
| Telemetry | Raw ADC values for all inputs |
| RX Viewer | Decoded telemetry reply from the active receiver |
| Target Select | Browse and select a saved receiver MAC |
| Settings | 11-item scrollable configuration menu |
| Edit Param | Adjust the highlighted setting with the encoder |
| About | Firmware version + device MAC address |

---

## Hardware Pins

| Function | GPIO |
|---|--:|
| Joystick X (`PIN_JOY_VRX`) | 35 |
| Joystick Y (`PIN_JOY_VRY`) | 33 |
| Potentiometer (`PIN_POT`) | 32 |
| Encoder CLK | 27 |
| Encoder DT | 14 |
| Encoder SW | 13 |
| Toggle 1 | 26 |
| Toggle 2 | 25 |
| Push 1 | 19 |
| Push 2 | 15 |
| OLED SDA | 21 |
| OLED SCL | 22 |
| Yellow LED / Vibrator | 23 |
| Green LED / Buzzer | 18 |

---

## Packet Format

```cpp
typedef struct __attribute__((packed)) struct_message {
    uint16_t joyX;        // Joystick X (0–4095, center ≈ 2048)
    uint16_t joyY;        // Joystick Y (0–4095, center ≈ 2048)
    uint16_t potValue;    // Potentiometer (0–4095)
    bool     toggle1;     // Toggle switch 1
    bool     toggle2;     // Toggle switch 2
    bool     push1;       // Push button 1
    bool     push2;       // Push button 2
    uint8_t  verifyKey;   // Pairing key (0 = accept all)
    uint8_t  addrLedMode; // Addressable LED mode (0–9)
} struct_message;  // 12 bytes
```

```cpp
typedef struct __attribute__((packed)) rx_message {
    uint8_t data1;   // Active LED mode on receiver
    uint8_t data2;   // Motor-sync state
    uint8_t data3;   // Battery level (0–255)
} rx_message;  // 3 bytes
```

---

## Project Structure

```
esp-now-oled/v2.0/
├── include/
│   ├── Config.h            # Pin definitions, structs, enums
│   ├── DisplayManager.h    # OLED UI + sliding-window settings renderer
│   ├── FeedbackManager.h   # Non-blocking haptic/audio sequences
│   ├── InputManager.h      # ADC + encoder + button input
│   ├── NetworkManager.h    # ESP-NOW TX/RX + link monitoring
│   ├── PortalManager.h     # Captive portal AP for pairing
│   └── StorageManager.h    # NVS Preferences persistence
└── src/
    ├── main.cpp            # FSM, encoder nav, TX loop
    ├── DisplayManager.cpp
    ├── FeedbackManager.cpp
    ├── InputManager.cpp
    ├── NetworkManager.cpp
    ├── PortalManager.cpp
    └── StorageManager.cpp
```

---

## Build & Flash

```bash
cd firmware/transmitter/esp-now-oled/v2.0
pio run --target upload
pio device monitor
```

---

## Pairing a New Receiver

1. Navigate to **Settings → Add Tgt (AP)**
2. Connect phone/laptop to Wi-Fi: SSID `ESP-NOW-REMOTE` / password `12345678`
3. Open `http://192.168.4.1`
4. Enter a name and the receiver's MAC address (printed on its serial monitor at boot)
5. Save — the transmitter reboots and loads the new target automatically
6. Navigate to **Target Select** to switch between saved receivers (up to 5)
