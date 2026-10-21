# ESP-NOW OLED Remote Controller — v2.1

A high-performance ESP32-based wireless remote controller using the ESP-NOW protocol. Combines the modern architecture and stability improvements of **v2.0** with the legacy-compatible 10-byte packet configuration and settings layout of **v1.3**.

---

## What's in v2.1

`v2.1` is designed for seamless backwards-compatibility with receivers using the standard 7-field 10-byte payload while preserving all architectural improvements from v2.0:

### 1️⃣ v1.3 Configuration & Payload Compatibility
- **10-Byte Legacy Data Packet**: Uses the standard 7-field `struct_message` (`joyX`, `joyY`, `potValue`, `toggle1`, `toggle2`, `push1`, `push2`) without `verifyKey` or `addrLedMode`.
- **Broadcast Out-of-the-Box**: Defaults to ESP-NOW broadcast (`FF:FF:FF:FF:FF:FF`) when no targets are saved, enabling instant operation without pre-pairing.
- **10-Item Settings Menu**: Settings menu items 0–9 from v1.3 (no receiver addressable LED mode setting).
- **Simple Captive Portal**: Web configuration captive portal requires only Target Name and MAC address.

### 2️⃣ v2.0 Codebase & Engine
- **Non-Blocking Feedback Engine**: Multi-sequence `FeedbackManager` with haptic vibration and audio buzzer alerts for buttons, toggles, connection drops, and recoveries without stalling the main loop.
- **Display Optimization**: Hash-based dirty-redraw on the 128×64 SSD1306 OLED display with dynamic sliding-window scrolling.
- **Input Filtering & Precision**: Exponential Moving Average (EMA, $\alpha=0.15$) noise filtering with hysteresis deadzone on analog ADC inputs.
- **Dynamic Peer Target Management**: Robust ESP-NOW peer addition/deletion with status checking and NVS persistence.

---

## Settings Menu

| # | Item | Description |
|---|------|-------------|
| 0 | Add Tgt (AP) | Start captive portal to add a new receiver MAC |
| 1 | Deadzone | ADC deadzone (0–2000) |
| 2 | Inv X/Y | Toggle axis inversion |
| 3 | Factory Rst | Wipe all stored settings and reboot |
| 4 | Tx Rate | Packets per second (5–250 Hz) |
| 5 | Rx Mode | Enable/disable telemetry receive |
| 6 | LED Maps | Transmitter status LED behaviour |
| 7 | Out Type | LEDs vs Vibrator/Buzzer output pins |
| 8 | Feedback | Silent / Vib / Buzz / Both |
| 9 | Test Haptic | Fire connection-loss feedback pattern for testing |

---

## Display Screens

| Screen | Description |
|--------|-------------|
| HUD | Real-time joystick bars, button state, TX/RX status |
| Telemetry | Raw ADC values for all inputs |
| RX Viewer | Decoded telemetry reply from the active receiver |
| Target Select | Browse and select a saved receiver MAC |
| Settings | 10-item scrollable configuration menu |
| Edit Param | Adjust the highlighted setting with the encoder |
| About | Firmware version (`v2.1`) + device MAC address |

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

### Transmitter → Receiver (10 bytes)

```cpp
typedef struct struct_message {
    uint16_t joyX;        // Joystick X (0–4095, center ≈ 2048)
    uint16_t joyY;        // Joystick Y (0–4095, center ≈ 2048)
    uint16_t potValue;    // Potentiometer (0–4095)
    bool     toggle1;     // Toggle switch 1
    bool     toggle2;     // Toggle switch 2
    bool     push1;       // Push button 1
    bool     push2;       // Push button 2
} struct_message;  // 10 bytes
```

### Receiver → Transmitter (telemetry reply)

```cpp
typedef struct rx_message {
    uint8_t data1;
    uint8_t data2;
    uint8_t data3;
} rx_message;  // 3 bytes
```

---

## Build & Flash

```bash
cd firmware/transmitter/esp-now-oled/v2.1
pio run --target upload
pio device monitor
```
