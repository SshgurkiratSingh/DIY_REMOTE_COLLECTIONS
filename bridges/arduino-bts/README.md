# Arduino BTS Bridge

## Purpose
Standalone Arduino motor bridge logic for BTS7960-style drivers.

## Hardware Required
- Arduino-compatible board
- BTS7960 (or equivalent dual-direction motor driver)
- Proper motor power stage and grounding

## Build/Upload
```bash
cd bridges/arduino-bts
pio run
pio run --target upload
```

## Notes
Useful as downstream bridge logic when control input is handled by another device.
