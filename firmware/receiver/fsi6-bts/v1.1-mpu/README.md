# FS-i6 BTS Receiver v1.1-mpu

## Purpose
FS-i6 RC bridge receiver with MPU6050-assisted drive behavior.

## Build/Upload
```bash
cd firmware/receiver/fsi6-bts/v1.1-mpu
pio run
pio run --target upload
```

## Notes
Adds IMU processing on top of FS-i6 RC control pipeline.
