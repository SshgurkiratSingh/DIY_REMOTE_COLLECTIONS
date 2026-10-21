# Changelog

## [v2.1.0] - October 21, 2026

### Overview
Firmware `v2.1` merges the codebase of `v2.0` with the standard 10-byte packet configuration from `v1.3`.

### Key Changes
- **Configuration (v1.3 Compatibility)**:
  - Reverted `struct_message` to 10 bytes (`joyX`, `joyY`, `potValue`, `toggle1`, `toggle2`, `push1`, `push2`).
  - Removed `verifyKey` and `addrLedMode` fields to enable instant communication with standard receivers (such as `esp-now-legacy`, `direct-bts/v1.0-v2.1`, and other 10-byte ESP-NOW receivers).
  - Maintained 10-item settings menu (items 0–9).
  - Out-of-the-box broadcast default (`FF:FF:FF:FF:FF:FF`) when no targets are saved.
  - Simplified captive portal form to Name and MAC address.
- **Codebase (v2.0 Architecture)**:
  - Non-blocking `FeedbackManager` with multi-step sequence polling for haptic and audio profiles.
  - Dynamic sliding-window menu scrolling in `DisplayManager`.
  - Hash-based change detection for OLED refresh.
  - Robust target management and connection state monitoring in `NetworkManager`.
  - Exponential Moving Average (EMA) filtering and hysteresis deadzone in `InputManager`.
