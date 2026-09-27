# ESP-NOW Receiver v2.0-led — Eye Matrix Edition

**L298N Motor Driver + Dual WS2812B LED Eye Matrix (10 LEDs each)**

ESP32-based receiver compatible with the **Transmitter v2.0** (`esp-now-oled/v2.0`). Combines dual DC motor control via L298N with a pair of 10-LED WS2812B strips shaped as expressive robot eyes. LED mode and brightness are controlled entirely from the remote — no button presses on the car needed.

---

## Compatible Transmitter

- `firmware/transmitter/esp-now-oled/v2.0`
- No transmitter code changes needed — receiver reads `addrLedMode` from each incoming packet

---

## Eye Layout

Each strip has 10 LEDs arranged in 3 physical lines:

```
Line 1  : LEDs  0,  1,  2
[hidden]: LED   3         ← routing LED, always off in software
Line 2  : LEDs  4,  5,  6
[hidden]: LED   7         ← routing LED, always off in software
Line 3  : LEDs  8,  9
```

> Hidden LEDs (index 3 and 7) carry wire routing but are always masked to black before each `FastLED.show()`.

---

## Wiring

```
ESP32 DevKit → L298N Motor Driver
  GPIO 25 → ENA (Motor A PWM)
  GPIO 26 → IN1 (Motor A direction)
  GPIO 27 → IN2 (Motor A direction)
  GPIO 14 → ENB (Motor B PWM)
  GPIO 12 → IN3 (Motor B direction)
  GPIO 13 → IN4 (Motor B direction)

ESP32 DevKit → WS2812B Left Eye
  GPIO 15 → Data In
  5V      → VCC  (use an external 5 V UBEC, not ESP32 3.3 V)
  GND     → GND  (common ground with ESP32)

ESP32 DevKit → WS2812B Right Eye
  GPIO 16 → Data In
  5V      → VCC
  GND     → GND

ESP32 DevKit → Misc
  GPIO  2 → Onboard LED (signal status indicator)
  GPIO 34 → Battery voltage divider (optional, ADC1)
```

---

## Remote Controls

| Remote Input | Function |
|---|---|
| Joystick Y | Throttle (forward / backward) |
| Joystick X | Steering (left / right) |
| Potentiometer | LED brightness (floor at 10, so strip never goes dark) |
| Toggle Switch 1 | Motor-LED sync mode ON/OFF |
| Toggle Switch 2 | Motor enable / disable (safety kill switch) |
| Settings → Addr LED (item 10) | Select LED mode 0–9 via encoder |

---

## LED Modes (10)

| # | Name | Description |
|---|------|-------------|
| 0 | Solid Headlights | All visible LEDs white (static, redraws only on mode change) |
| 1 | Angry Eyes | Top + middle lines solid red, bottom line off |
| 2 | Scanning Pupil | Column sweeps left↔right (signed bounce, eye-accurate) |
| 3 | Natural Blinking | White with a ~150 ms blink every 3 s |
| 4 | Sleepy Breathing | Cyan sine-wave pulse on lower two lines only |
| 5 | Rainbow Flow | Hue shifts across LEDs continuously |
| 6 | Hypnotic Lines | Cycles top → middle → bottom with rotating colour |
| 7 | Fire Flicker | Random warm-spectrum flicker per LED |
| 8 | Police Strobe | Left eye red / right eye blue, alternating |
| 9 | Cyber Sparkle | Random-hue digital sparkle with fade trail |

---

## Motor-LED Sync Mode (Toggle 1 ON)

When enabled, LEDs ignore the selected mode and instead reflect driving state:

| Driving State | LED Behaviour |
|---|---|
| Idle | Gentle green breathing |
| Forward | Green sparkle sweep (speed-scaled) |
| Reverse | Red sparkle sweep (speed-scaled) |
| Turning only | Orange blink on the turning side |
| Full speed (>86%) | Rapid rainbow burst |
| Braking | Red flash |

---

## Signal Loss

If no packet is received for **>1 second**:
- Motors stop immediately
- Both eye strips show a slow red breathing pulse
- Status LED (GPIO 2) turns off

---

## Packet Format

```cpp
// Must match transmitter v2.0 Config.h exactly
typedef struct __attribute__((packed)) struct_message {
    uint16_t joyX;        // 0–4095, center ≈ 2048
    uint16_t joyY;        // 0–4095, center ≈ 2048
    uint16_t potValue;    // 0–4095 → mapped to LED brightness 10–255
    bool     toggle1;     // motor-LED sync
    bool     toggle2;     // motor enable (kill switch)
    bool     push1;
    bool     push2;
    uint8_t  verifyKey;   // pairing key (0 = accept all)
    uint8_t  addrLedMode; // LED mode 0–9
} struct_message;  // 12 bytes
```

---

## Building & Flashing

```bash
cd firmware/receiver/direct-bts/v2.0-led
pio run                   # compile
pio run --target upload   # compile + flash
pio device monitor        # serial monitor @ 115200
```

The serial output on boot prints the receiver MAC address and the `struct_message` size (must match transmitter).

---

## Pairing

1. Flash this firmware and note the MAC address printed to serial
2. On the transmitter OLED: **Settings → Add Tgt(AP)**
3. Connect to Wi-Fi `ESP-NOW-REMOTE` / password `12345678`
4. Open `http://192.168.4.1`, enter the MAC address and a name, and save
5. Select the new target from the Target Select menu
6. The transmitter will automatically unicast to this receiver
