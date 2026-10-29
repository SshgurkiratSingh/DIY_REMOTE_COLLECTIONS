#pragma once

#include <Arduino.h>

// ============================================================================
//  LFR v3.0 - 5-IR Sensor Line Following Robot Configuration
// ============================================================================

#define LFR_VERSION             "v3.0 (5-IR Array)"

// ----------------------------------------------------------------------------
//  TB6612FNG Dual H-Bridge Motor Driver Pins
// ----------------------------------------------------------------------------
#define MOTOR_PWMA_PIN          13   // PWM speed control for Left Motor (Motor A)
#define MOTOR_AIN1_PIN          27   // Direction control 1 for Left Motor (Ain 1 = 27)
#define MOTOR_AIN2_PIN          14   // Direction control 2 for Left Motor (Ain 2 = 14)
#define MOTOR_STBY_PIN          26   // Standby control (HIGH = enabled, LOW = standby)
#define MOTOR_BIN1_PIN          25   // Direction control 1 for Right Motor (Bin 1)
#define MOTOR_BIN2_PIN          16   // Direction control 2 for Right Motor (Bin 2)
#define MOTOR_PWMB_PIN          12   // PWM speed control for Right Motor (Motor B)

// Aliases for compatibility
#define ENA_PIN                 MOTOR_PWMA_PIN
#define IN1_PIN                 MOTOR_AIN1_PIN
#define IN2_PIN                 MOTOR_AIN2_PIN
#define STBY_PIN                MOTOR_STBY_PIN
#define IN3_PIN                 MOTOR_BIN1_PIN
#define IN4_PIN                 MOTOR_BIN2_PIN
#define ENB_PIN                 MOTOR_PWMB_PIN

// ----------------------------------------------------------------------------
//  5x IR Line Tracking Sensor Pins (Left S0 to Right S4)
//  All on ADC1 (Safe with concurrent active WiFi)
//  Physical Robot Left-to-Right order: [39, 34, 35, 32, 33]
//  Center sensor is GPIO 35 (Index 2)
// ----------------------------------------------------------------------------
#define NUM_SENSORS             5

// Default sensor physical mapping (Front View: S0=Far Left ... S4=Far Right):
#define SENSOR_PIN_0            39   // S0: Far Left  (VN / GPIO 39)
#define SENSOR_PIN_1            34   // S1: Near Left (GPIO 34)
#define SENSOR_PIN_2            35   // S2: Center    (GPIO 35)
#define SENSOR_PIN_3            32   // S3: Near Right(GPIO 32)
#define SENSOR_PIN_4            33   // S4: Far Right (GPIO 33)

#define SENSOR_CENTER_INDEX     2    // Index of the center sensor (S2 - GPIO 35)
#define SENSOR_CENTER_PIN       SENSOR_PIN_2

static const uint8_t SENSOR_PINS[NUM_SENSORS] = {
    SENSOR_PIN_0, SENSOR_PIN_1, SENSOR_PIN_2, SENSOR_PIN_3, SENSOR_PIN_4
};

// Indicators & Buttons
#define STATUS_LED_PIN          2    // Built-in status LED
#define BOOT_BUTTON_PIN         0    // ESP32 BOOT button (active LOW)

// ============================================================================
//  Motor PWM Configuration (LEDC)
// ============================================================================
#define MOTOR_PWM_FREQ          5000 // 5 kHz PWM frequency
#define MOTOR_PWM_RES_BITS      8    // 8-bit resolution (0 - 255)
#define MOTOR_PWM_CHANNEL_LEFT  0    // LEDC channel for left motor
#define MOTOR_PWM_CHANNEL_RIGHT 1    // LEDC channel for right motor

// ============================================================================
//  WiFi Captive Portal Configuration
// ============================================================================
#define AP_SSID                 "LFR-5IR-Calibrate"
#define AP_PASSWORD             ""   // Open network (no password required)
#define AP_CHANNEL              1
#define AP_MAX_CONN             4
#define DNS_PORT                53
#define HTTP_PORT               80

// ============================================================================
//  Boot & Calibration Triggers
// ============================================================================
#define CALIBRATION_BOOT_INTERVAL 5    // Calibrate every 5th boot
#define FORCE_CALIB_HOLD_TIME_MS  1000 // Hold boot button 1s at boot to force calibration
#define PREFERENCES_NAMESPACE     "lfr_cal"

// ============================================================================
//  Navigation Modes (Dynamic & Unknown Map Ready)
// ============================================================================
#define NAV_MODE_AUTONOMOUS_LSRB 0    // Autonomous Left-Hand Rule (LSRB) - for unknown mazes
#define NAV_MODE_AUTONOMOUS_RSLB 1    // Autonomous Right-Hand Rule (RSLB)
#define NAV_MODE_CUSTOM_PATH     2    // Pre-programmed turn sequence
#define NAV_MODE_PID_LINE        3    // Standard PID Line Follower (smooth continuous tracking)

#define DEFAULT_NAV_MODE         NAV_MODE_PID_LINE
#define MAX_PATH_STEPS           64

// Default route from Rulebook Map
#define DEFAULT_RULEBOOK_ROUTE   "RRLLRLSSLLRRLL"

// ============================================================================
//  Default Parameters
// ============================================================================
// With 5 sensors (1.5cm spacing), position error ranges from -2000 to +2000.
#define DEFAULT_KP              16.0f
#define DEFAULT_KI              0.04f
#define DEFAULT_KD              12.0f

#define DEFAULT_BASE_SPEED      110  // Controlled, stable baseline speed
#define DEFAULT_MAX_SPEED       220
#define DEFAULT_MIN_SPEED       0    // 0 prevents violent spin-out; smooth differential braking (set negative in portal if pivot turns needed)

#define DEFAULT_RAW_MIN         0
#define DEFAULT_RAW_MAX         4095

// Scaling factor for PID calculation: correction = (P + I + D) / PID_SCALE_DIVISOR
#define PID_SCALE_DIVISOR       200.0f
#define PID_ANTI_WINDUP_LIMIT   10000.0f
#define LINE_NOISE_FLOOR        60   // Reject background noise on white surface
#define LINE_MIN_SUM            100  // Minimum cumulative norm to declare line detected

// ============================================================================
//  Timing & Maze Navigation Configuration
// ============================================================================
#define LOOP_RATE_HZ            100    // Main PID loop frequency (10ms)
#define MOTOR_SAFETY_TIMEOUT_MS 500    // Stop motors if no sensor updates for >500ms
#define CALIBRATION_DURATION_MS 10000  // 10 seconds calibration sweep

#define JUNCTION_INCH_TIME_MS   110    // Inching time to align wheels with junction
#define TURN_SPEED              130    // PWM speed during pivot turns and U-turns
#define TURN_TIMEOUT_MS         2000   // Timeout for in-place turns
#define END_ZONE_INCH_MS        450    // Inching duration to confirm Zone B large black square
#define CHECKPOINT_DEBOUNCE_MS  1000   // Minimum time between checkpoint triggers
#define LINE_LOST_TIMEOUT_MS    1500   // Search timeout when line is lost

// Helper to read sensor raw value (analog for ADC pins, digital fallback for non-ADC pins)
static inline uint16_t readSensorRaw(uint8_t pin) {
    if (pin == 32 || pin == 33 || pin == 34 || pin == 35 || pin == 36 || pin == 39) {
        return analogRead(pin);
    }
    if (pin == 4 || pin == 15 || pin == 2) {
        int val = analogRead(pin);
        if (val >= 0) return (uint16_t)val;
        return digitalRead(pin) ? 4095 : 0;
    }
    return digitalRead(pin) ? 4095 : 0;
}
