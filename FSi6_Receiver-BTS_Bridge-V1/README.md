# FSi6 Receiver to BTS7960 Bridge

An Arduino-based project that reads IBus telemetry from a FlySky FS-i6 receiver and translates it into PWM signals for dual BTS7960 motor drivers. The system features multiple drive logics, dynamic speed control, and failsafe handling.

## Hardware Requirements

* **Microcontroller**: Arduino Uno (or compatible AVR board)
* **Receiver**: FlySky FS-iA6B (or any IBus-capable receiver paired with an FS-i6 transmitter) 
* **Motor Drivers**: 2x BTS7960 (43A High Power Motor Driver Modules)
* **Motors**: 2x DC Motors

## Pin Mapping

### BTS7960 Motor Drivers

**Left Motor:**
* `RPWM` -> Pin 5
* `LPWM` -> Pin 6
* `L_EN` -> Pin 7
* `R_EN` -> Pin 8

**Right Motor:**
* `RPWM` -> Pin 9
* `LPWM` -> Pin 10
* `L_EN` -> Pin 11
* `R_EN` -> Pin 12

### IBus Receiver
* `IBus / Serial RX` -> Arduino Pin 0 (Hardware Serial RX)

## Channel Mapping & Drive Modes

The logic bridges standard 6-channel RC input into comprehensive motor control:

| Channel | Function | Description |
| :--- | :--- | :--- |
| **CH 1** | Steering / Right Tank | X-axis of the right stick. |
| **CH 2** | Throttle / Left Tank | Y-axis of the right stick (or left stick depending on your Tx setup). |
| **CH 3** | Speed Scale | Dial/Knob used to scale the overall max speed natively. |
| **CH 4** | Burst Mode | Switch to enable a speed boost multiplier. |
| **CH 5** | Drive Mode Switch | 3-position switch to change the translation logic constraint. |
| **CH 6** | Action Toggle | General purpose action button toggle. |

### Available Drive Modes (Controlled via CH 5)

1. **Mode 1 (Switch High - Direct Tank Drive)**: Ch2 controls the Left motor directly, Ch1 controls the Right motor directly.
2. **Mode 2 (Switch Mid - Standard Arcade)**: Mixes Ch1 and Ch2 to provide combined throttle and steering on a solitary stick.
3. **Mode 3 (Switch Low - Dynamic Speed Arcade)**: Implements arcade logic along with fine control and "burst mode" capabilities (toggled via Ch4) effectively amplifying or restricting the output speed.

## Failsafe

The system natively utilizes the IBusBM timeout and error checking logic. If the connection drops or times out, the `failsafe` flag is triggered, which safely commands the motor controllers to an immediate `stop()` state.

## Software Setup & Building

This project is configured using **PlatformIO**. 

1. Install [PlatformIO](https://platformio.org/).
2. Clone or open this repository in VS Code.
3. Dependencies (like the `IBusBM` library) will be managed and downloaded automatically via `platformio.ini`.
4. Run `pio run` to build.
5. Run `pio run -t upload` to flash the Arduino.

*Note: Since the IBus receiver actively uses the hardware RX pin (Pin 0), you may need to unplug the receiver's RX wire when flashing the Arduino to prevent upload interference.*