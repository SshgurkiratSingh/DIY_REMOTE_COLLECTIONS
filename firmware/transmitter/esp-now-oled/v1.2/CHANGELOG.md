# Changelog

## [v1.2.0] - March 31, 2026

### Major Improvements

#### 1. OLED Display Optimization - Change Detection Only

- Implemented hash-based change detection for display content
- OLED screen only refreshes when actual data changes (not every frame)
- Significantly reduces OLED wear and extends display lifespan
- Improves system responsiveness by reducing I2C overhead
- LED status indicators continue updating every iteration for real-time feedback
- Uses XOR-based hash computation for fast change detection

**Benefits:**

- Lower power consumption
- Reduced OLED burn-in risk
- I2C bus less congested

#### 2. ADC Noise Filtering - Exponential Moving Average

- Implemented **EMA (Exponential Moving Average)** filter for all analog inputs
- Filters: Joystick X, Joystick Y, and Potentiometer
- Reduces ~10 unit jitter on ESP32 ADC inputs to smooth, stable readings
- Tunable smoothing factor: `EMA_ALPHA = 0.15` (adjustable for responsiveness)

**Filter Equation:**

```
filtered_value = (EMA_ALPHA × raw_value) + ((1 - EMA_ALPHA) × previous_filtered_value)
```

**Configuration:**

- `EMA_ALPHA = 0.15`: Balances smoothing (85% history) with responsiveness (15% new data)
- Adjust lower for smoother but slower response
- Adjust higher for faster but noisier response

**Additional ADC Improvements:**

- Set ADC attenuation to `ADC_11db` for better stability in mid-range
- Improves linearity and reduces noise floor

#### 3. Improved Deadzone with Hysteresis

- Replaced simple threshold deadzone with **hysteresis-based system**
- Prevents rapid in/out oscillations around deadzone boundary (causes jitter)
- Only exits deadzone when movement exceeds: `deadzone + HYSTERESIS_MARGIN (8 units)`
- Only re-enters deadzone when movement falls below deadzone threshold

**Hysteresis Logic:**

- **Inside Deadzone**: Joystick output = centerX/centerY
- **Outside Deadzone**: Joystick output = filtered reading
- **Transition Zone**: Small margin (8 units) prevents fluttering

**Configuration:**

- `HYSTERESIS_MARGIN = 8`: Size of transition zone (prevents jitter)
- Can be adjusted in [InputManager.h](include/InputManager.h)

**Benefits:**

- Smooth, reliable joystick control
- No center drift or vibration
- Better gaming/control precision

### Technical Details

#### Files Modified:

- [include/DisplayManager.h](include/DisplayManager.h)

  - Added `computeDataHash()` method
  - Added member variables for change detection
- [include/InputManager.h](include/InputManager.h)

  - Added EMA filter state variables
  - Added hysteresis state tracking
  - Added tunable constants for EMA_ALPHA and HYSTERESIS_MARGIN
- [src/DisplayManager.cpp](src/DisplayManager.cpp)

  - Implemented change detection with hash-based comparison
  - Updated `update()` method to skip display refresh on no changes
  - Moved LED updates to `update_leds` label for always-on operation
- [src/InputManager.cpp](src/InputManager.cpp)

  - Implemented EMA filter in ADC readings
  - Added hysteresis state machine for deadzone
  - Set optimal ADC attenuation
- [include/Config.h](include/Config.h)

  - Version bumped to v1.2.0

### Compilation Status

✅ **Build Successful**

- RAM Usage: 14.3% (46984/327680 bytes)
- Flash Usage: 66.0% (865693/1310720 bytes)

### Testing Recommendations

1. **ADC Stability Test**: Monitor joystick values with deadzone disabled - verify ±1-2 unit stability around center
2. **Deadzone Test**: Verify joystick stays centered with small movements within deadzone range
3. **Display Performance**: Check that OLED updates smoothly when moving controls
4. **Power Consumption**: Compare current draw before/after (should be lower with optimized display)

### Migration Notes

- **No Breaking Changes**: Configurations from v1.1.0 remain compatible
- **No New Configuration Required**: All improvements use optimal defaults
- **Optional Tuning**: Adjust `EMA_ALPHA` in InputManager.h and `HYSTERESIS_MARGIN` if needed for specific hardware
