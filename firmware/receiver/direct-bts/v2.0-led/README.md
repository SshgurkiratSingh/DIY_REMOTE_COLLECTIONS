# ESP-NOW Receiver v2.0-led

**L298N Motor Driver + Dual WS2812B Addressable LED Strips (10 LEDs each)**

ESP32-based receiver that combines dual DC motor control via L298N with dual 10-LED WS2812B addressable LED strips featuring 12 animation modes and motor-LED sync, all controllable via the ESP-NOW v2.0 remote.

## Compatible Transmitter

- `firmware/transmitter/esp-now-oled/v2.0` (ESP-NOW OLED Remote)
- No transmitter firmware changes needed

## Wiring

```
ESP32 DevKit → L298N Motor Driver
  GPIO 25 → ENA (Motor A PWM)
  GPIO 26 → IN1 (Motor A direction)
  GPIO 27 → IN2 (Motor A direction)
  GPIO 14 → ENB (Motor B PWM)
  GPIO 12 → IN3 (Motor B direction)
  GPIO 13 → IN4 (Motor B direction)

ESP32 DevKit → WS2812B LED Strips (Left & Right)
  GPIO 15 → Left Strip Data In
  GPIO 16 → Right Strip Data In
  5V       → VCC (use external 5V, not ESP32 3.3V)
  GND      → GND (common ground with ESP32)

ESP32 DevKit → Other
  GPIO  2 → Onboard LED (status indicator)
  GPIO 34 → Battery voltage divider (optional)
```

## Controls

| Remote Input | Function |
|---|---|
| Joystick Y | Throttle (forward/backward) |
| Joystick X | Steering (left/right) |
| Potentiometer | LED brightness (twist to dim/brighten) |
| Push Button 1 | Cycle LED mode forward |
| Push Button 2 | Cycle LED mode backward |
| Toggle Switch 1 | Motor-LED sync mode ON/OFF |
| Toggle Switch 2 | Motor enable/disable (safety kill) |

## LED Modes (12)

| # | Mode | Description |
|---|---|---|
| 0 | Solid Color Cycle | All LEDs same color, rotating hue |
| 1 | Rainbow Wave | Rainbow flowing across LEDs |
| 2 | Breathing Pulse | Smooth sine-wave pulse |
| 3 | Knight Rider | Red bounce with fade trail |
| 4 | Strobe Flash | Fast white strobe |
| 5 | Fire Flicker | Random warm flame effect |
| 6 | Color Wipe | Fill one LED at a time |
| 7 | Sparkle | Random twinkle |
| 8 | Police Siren | Alternating red/blue flash |
| 9 | Meteor Rain | Bright head with decay trail |
| 10 | Theater Chase | Marquee chase in rainbow |
| 11 | Static White | Steady white (utility) |

## Motor-LED Sync Mode

When Toggle 1 is ON, LEDs react to motor state:

| Driving State | LED Behavior |
|---|---|
| Idle | Gentle green breathing |
| Forward | Green sweep back→front |
| Reverse | Red sweep front→back |
| Turning | Orange tint on turn side |
| Full speed | Rapid rainbow pulse |
| Braking | Red flash burst |

## Signal Loss

When the remote signal is lost (>1 second timeout):
- Motors stop immediately
- LEDs show slow red breathing pulse
- Status LED turns off

## Building

```bash
cd firmware/receiver/direct-bts/v2.0-led
pio run                    # Compile
pio run --target upload    # Flash
pio device monitor         # Serial monitor
```

## Pairing

1. Flash this firmware and note the MAC address from serial output
2. On the transmitter, go to Settings → Add Target (AP)
3. Connect to WiFi `ESP-NOW-REMOTE` / password `12345678`
4. Enter the receiver's MAC address and save
5. Select the target from the target menu
