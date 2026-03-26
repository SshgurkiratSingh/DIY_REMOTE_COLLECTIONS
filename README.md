# REMOTE_ESPNOW_LORA_RF_BLE

An ESP32-based remote control collection centered around an ESP-NOW handheld transmitter with OLED UI, rotary-encoder navigation, persistent target storage, and an optional telemetry reply path from the receiver.

The repository currently contains multiple firmware snapshots:

- `ESP_NOW_V1.1_OLED`: main handheld transmitter with OLED UI and captive-portal target management
- `ESP_NOW_V1.0_OLED`: earlier OLED transmitter iteration
- `Receiver_v1.1.2_esp_now`: receiver that prints incoming controls and sends a small reply packet
- `Receiver_v1.1_esp_now`: earlier receiver iteration

## What This Project Does

The transmitter reads:

- 2-axis joystick
- 1 potentiometer
- 2 toggle switches
- 2 push buttons
- 1 rotary encoder with push switch

It then sends the live control state over ESP-NOW to a selected receiver. The OLED UI lets you:

- monitor joystick and switch activity
- browse saved receiver targets
- adjust deadzone, inversion, TX rate, telemetry mode, and LED mode
- view device info such as firmware version and MAC address

The current receiver firmware:

- listens for the transmitted control packet
- prints values to the serial monitor
- adds the sender as a peer dynamically
- sends back a compact reply packet for simple telemetry testing

## Repository Layout

```text
.
├── ESP_NOW_V1.1_OLED/          # Main transmitter firmware
├── ESP_NOW_V1.0_OLED/          # Earlier transmitter version
├── Receiver_v1.1.2_esp_now/    # Main receiver firmware
├── Receiver_v1.1_esp_now/      # Earlier receiver version
├── docs/                       # GitHub Pages website
└── .github/workflows/          # GitHub Pages deployment workflow
```

## Main Firmware Notes

### Transmitter: `ESP_NOW_V1.1_OLED`

Key features implemented in code:

- SSD1306 128x64 OLED interface
- rotary encoder driven menu system
- NVS persistence using `Preferences`
- stored receiver target list with last-target recall
- captive portal access point for adding new receiver MAC addresses
- optional ESP-NOW receive callback for lightweight telemetry

Default persisted settings loaded by the firmware:

- `deadzone = 150`
- `centerX = 2048`
- `centerY = 2048`
- `invertX = false`
- `invertY = false`
- `txRateHz = 50`
- `rxEnabled = false`
- `ledMode = LED_TX_RX`

### Receiver: `Receiver_v1.1.2_esp_now`

Current behavior:

- boots in `WIFI_STA`
- initializes ESP-NOW
- prints its MAC address to serial
- validates incoming packet size
- logs joystick, potentiometer, and switch values
- sends a reply structure back to the sender

## Pin Mapping

The main transmitter pin configuration in `ESP_NOW_V1.1_OLED/include/Config.h` is:

| Function                     | Pin |
| ---------------------------- | --: |
| Joystick Y (`PIN_JOY_VRY`) |  33 |
| Potentiometer (`PIN_POT`)  |  32 |
| Joystick X (`PIN_JOY_VRX`) |  35 |
| Encoder CLK                  |  27 |
| Encoder DT                   |  14 |
| Encoder Switch               |  13 |
| Toggle 1                     |  26 |
| Toggle 2                     |  25 |
| Push 1                       |  19 |
| Push 2                       |  15 |
| OLED SDA                     |  21 |
| OLED SCL                     |  22 |
| Yellow LED                   |  23 |
| Green LED                    |  18 |

## Packet Format

Transmitter payload:

```cpp
typedef struct struct_message
{
    uint16_t joyX;
    uint16_t joyY;
    uint16_t potValue;
    bool toggle1;
    bool toggle2;
    bool push1;
    bool push2;
} struct_message;
```

Receiver reply packet:

```cpp
typedef struct struct_reply {
  uint8_t sensorValue;
  uint8_t counter;
  uint8_t flags;
} struct_reply;
```

## Building And Flashing

This repo uses PlatformIO.1. Install prerequisites

- VS Code with PlatformIO extension, or
- PlatformIO Core CLI

### 2. Open the desired firmware folder

Each firmware variant is its own standalone PlatformIO project. Open one of these folders directly:

- `ESP_NOW_V1.1_OLED`
- `Receiver_v1.1.2_esp_now`

### 3. Build

Examples:

```bash
cd ESP_NOW_V1.1_OLED
pio run
```

```bash
cd Receiver_v1.1.2_esp_now
pio run
```

### 4. Upload

```bash
pio run --target upload
```

### 5. Open serial monitor

```bash
pio device monitor
```

## Using The System

### Receiver setup

1. Flash `Receiver_v1.1.2_esp_now` to an ESP32.
2. Open the serial monitor at `115200`.
3. Note the printed receiver MAC address.

### Transmitter setup

1. Flash `ESP_NOW_V1.1_OLED` to another ESP32.
2. Enter the settings menu.
3. Choose `Add Tgt(AP)` to start the captive portal.
4. Connect to Wi-Fi SSID `ESP-NOW-REMOTE` with password `12345678`.
5. Open `192.168.4.1`.
6. Save a target name and the receiver MAC address.
7. Reboot occurs automatically after save.
8. Select the saved target from the target selection menu.
