# Direct BTS + MPU6050 v3.0

## Purpose
Direct ESP-NOW receiver with MPU6050 estimator and mode-aware drive control.

## Build/Upload
```bash
cd firmware/receiver/direct-bts-mpu6050/v3.0
pio run
pio run --target upload
```

## Notes
Includes command timeout safety and IMU update loop integration.
