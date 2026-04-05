# FS-i6 BTS Receiver MPU6050 v1

## Purpose
FS-i6 RC bridge receiver with MPU6050-assisted drive behavior.

## Build/Upload
```bash
cd firmware/receiver/fsi6-bts/mpu6050-v1
pio run
pio run --target upload
```

## Notes
Adds IMU processing on top of FS-i6 RC control pipeline.
