# FSi6 Receiver BTS Bridge & MPU6050 - V1

A robust, modular PlatformIO project for the Arduino Uno designed to bridge a FlySky FS-i6 receiver (via iBus), process spatial orientation data from an MPU6050 IMU, and independently control two high-power DC motors using BTS7960 motor drivers.

This project aims to provide a reliable base for robotics, RC vehicles, or automated drive systems that rely on 2-wheel drive or track-driven configurations, while integrating advanced IMU-based kinematics or stabilization.

---

## Hardware Requirements

- **Microcontroller**: Arduino Uno (or compatible Atmega328p board).
- **RC Transmitter/Receiver**: FlySky FS-i6 transmitter paired with an FS-iA6B (or similar) receiver capable of iBus output.
- **Motor Driver**: 2x BTS7960 43A High-Power Motor Driver Modules (or a dual-motor driver configuration matching the pinout).
- **Motors**: 2x DC Motors suitable for your chassis.
- **IMU**: MPU6050 6-DOF Accelerometer and Gyroscope.
- **Power Supply**: Dedicated battery/power supply for the motors (connected to BTS7960s) and appropriate step-down/BEC for the Arduino.

---

## Pinout & Wiring Guide

### Left Motor (BTS7960 - 1)

| BTS7960 Pin | Arduino Pin | Description      |
| ----------- | ----------- | ---------------- |
| `RPWM`    | `D5`      | Right PWM signal |
| `LPWM`    | `D6`      | Left PWM signal  |
| `R_EN`    | `D8`      | Right Enable     |
| `L_EN`    | `D7`      | Left Enable      |

### Right Motor (BTS7960 - 2)

| BTS7960 Pin | Arduino Pin | Description      |
| ----------- | ----------- | ---------------- |
| `RPWM`    | `D9`      | Right PWM signal |
| `LPWM`    | `D10`     | Left PWM signal  |
| `R_EN`    | `D12`     | Right Enable     |
| `L_EN`    | `D11`     | Left Enable      |

### MPU6050 (I2C)

| MPU6050 Pin | Arduino Pin |
| ----------- | ----------- |
| `VCC`     | `5V`      |
| `GND`     | `GND`     |
| `SDA`     | `A4`      |
| `SCL`     | `A5`      |

### FS-iA6B Receiver (iBus)

| Receiver Pin | Arduino Pin       | Description                              |
| ------------ | ----------------- | ---------------------------------------- |
| `Signal`   | `RX` (Pin 0/HW) | iBus signal wire out from Rx sensor port |
| `VCC`      | `5V`            | Power                                    |
| `GND`      | `GND`           | Ground                                   |

---

## Software Architecture

The codebase is engineered with modularity in mind, structured under `src/` and `include/` directories:

1. **`main.cpp`**: The primary entry point. Initializes hardware, updates sensor readings, and executes the core drive loop at 50Hz (20ms delay).
2. **`MotorController.cpp/h`**: Encapsulates the BTS7960 driver logic. Handles direction, speed mapping, and enabling/disabling of the H-bridges to ensure safe PWM limits.
3. **`IMUHandler.cpp/h`**: Wraps the `Adafruit MPU6050` capabilities. Normalizes and filters raw gyro/accelerometer data into usable spatial metrics (Yaw, Pitch, Roll).
4. **`Receiver.cpp/h`**: Reads and decodes the FlySky iBus protocol using the `IBusBM` library, converting RC channel values (1000-2000) into normalized drive metrics or mode switches.
5. **`DriveLogic.cpp/h`**: The operational "brain". Consumes normalized `ReceiverData` and `IMU` metrics to compute necessary motor outputs (e.g., differential steering, stabilization mix) and sends them to the `MotorController`.

---

## Dependencies

This project uses **PlatformIO** for dependency management. The following libraries are automatically installed via `platformio.ini`:

- [`bmellink/IBusBM@^1.1.4`](https://github.com/bmellink/IBusBM): For interpreting the FlySky iBus serial data.
- [`adafruit/Adafruit MPU6050@^2.2.6`](https://github.com/adafruit/Adafruit_MPU6050): Core driver for the MPU6050 IMU.
- [`adafruit/Adafruit Unified Sensor@^1.1.14`](https://github.com/adafruit/Adafruit_Sensor): Dependency required for Adafruit sensor compatibility.

---

## Setup & Installation

1. **Clone the Repository**
2. **Open in PlatformIO**:
   - Open the directory in **VS Code** with the **PlatformIO** extension installed.
3. **Build the Project**:
   - Click the PlatformIO "Build" icon (checkmark) at the bottom taskbar to verify successful compilation. PlatformIO will automatically fetch the required dependencies.
4. **Upload Firmware**:
   - Connect your Arduino Uno via USB.
   - Click the PlatformIO "Upload" icon (right arrow).
   - *(Note: If your RC Receiver is connected to Hardware RX Pin 0, you must **disconnect** the receiver's signal wire before uploading, as it interferes with USB serial flashing).*
5. **Monitor & Calibrate**:
   - Open the Serial Monitor (`115200` baud) to verify successful IMU initialization and iBus connection. Uncomment the debugging print statements in `main.cpp` to monitor live channel and sensor output if needed.

---

## Usage Notes

- Always ensure the BTS7960 `VCC` and Arduino `5V` share a common `GND` with the main motor battery power source, otherwise PWM signaling will be erratic.
- The default loop time is ~20ms (`delay(20);`). You can adjust this for faster PID control if you integrate advanced balancing logic within `DriveLogic`.
