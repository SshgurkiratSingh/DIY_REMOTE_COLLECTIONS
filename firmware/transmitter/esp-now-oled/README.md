# ESP-NOW OLED Transmitter Family

Versioned handheld transmitter firmware.

## Versions
- `v1.0/`: baseline transmitter architecture.
- `v1.1/`: stable menu + portal generation.
- `v1.2/`: optimized display update, EMA filtering, hysteresis deadzone.
- `v1.3/`: pre-release v2.0 with haptics & 10-byte payload.
- `v2.0/`: packed 12-byte payload with pairing key (`verifyKey`) and addressable LED mode (`addrLedMode`).
- `v2.1/`: modern v2.0 architecture + backwards-compatible v1.3 configuration (standard 10-byte payload).

## Recommended Default
- Use **`v2.0`** if using `direct-l298n/v2.0-led` or pairing-key receivers.
- Use **`v2.1`** if using legacy/standard 10-byte receivers (`esp-now-legacy`, `direct-bts/v1.0-v2.1`).
