#pragma once

#include <Arduino.h>

// ============================================================================
//  Hardware Pin Definitions
// ============================================================================

// L298N Motor Driver Pins
#define ENA_PIN                 25   // PWM speed control for Left Motor
#define IN1_PIN                 26   // Direction control 1 for Left Motor
#define IN2_PIN                 27   // Direction control 2 for Left Motor
#define ENB_PIN                 14   // PWM speed control for Right Motor
#define IN3_PIN                 12   // Direction control 1 for Right Motor
#define IN4_PIN                 13   // Direction control 2 for Right Motor

// 9-Channel Analog IR Sensor Array
//
// Layout (robot's perspective, left to right):
//   S0    S1    S2    S3    S4    S5  | S6    S7    S8
//  [FL]  [L]  [CL]  [C]  [CR]  [R]  | [FR] [FFL] [FFR]
//
// ADC1 sensors (GPIO 32-39) — safe when WiFi is active (Calibration mode)
// ADC2 sensors (GPIO 2/4/15)— ONLY safe when WiFi is OFF (Running mode only)
//
#define NUM_SENSORS             9
#define ADC1_SENSOR_COUNT       6    // S0-S5: readable during calibration (WiFi on)
#define ADC2_SENSOR_COUNT       3    // S6-S8: only readable during running (WiFi off)

// ADC1 sensors — positions: Far-Left to Right
#define SENSOR_S0_PIN           39   // ADC1_CH3  Far Left        (WiFi-safe)
#define SENSOR_S1_PIN           36   // ADC1_CH0  Left            (WiFi-safe)
#define SENSOR_S2_PIN           35   // ADC1_CH7  Centre-Left     (WiFi-safe)
#define SENSOR_S3_PIN           34   // ADC1_CH6  Centre          (WiFi-safe)
#define SENSOR_S4_PIN           33   // ADC1_CH5  Centre-Right    (WiFi-safe)
#define SENSOR_S5_PIN           32   // ADC1_CH4  Right           (WiFi-safe)

// ADC2 sensors — positions: Far-Right extension
// WARNING: These return garbage/errors while WiFi radio is active.
//          They are ONLY read in MODE_RUNNING (WiFi is fully OFF).
//          GPIO2 doubles as STATUS_LED in calibration mode; that's fine because
//          in running mode the LED is simply left LOW and GPIO2 is used as ADC.
#define SENSOR_S6_PIN           4    // ADC2_CH0  Far Right       (running-only)
#define SENSOR_S7_PIN           15   // ADC2_CH3  Far-Far Right   (running-only)
#define SENSOR_S8_PIN           2    // ADC2_CH2  Outer Right     (running-only, shares STATUS_LED)

// Weights for each sensor in the position calculation
// Range: -4000 (S0 far-left) to +4000 (S8 far-right), step 1000
// Position = sum(weight[i] * norm[i]) / sum(norm[i])
#define SENSOR_WEIGHTS          {-4000, -3000, -1500, 0, 1500, 3000, 4000, 5000, 6000}

// Legacy aliases (kept for compatibility with turn detection in pid.cpp)
#define SENSOR_LEFT_PIN         SENSOR_S1_PIN
#define SENSOR_CENTER_PIN       SENSOR_S3_PIN
#define SENSOR_RIGHT_PIN        SENSOR_S5_PIN

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
#define AP_SSID                 "LFR-Calibrate"
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
#define NAV_MODE_AUTONOMOUS_LSRB 0    // Autonomous Left-Hand Rule (LSRB) - default for unknown/changed maps
#define NAV_MODE_AUTONOMOUS_RSLB 1    // Autonomous Right-Hand Rule (RSLB)
#define NAV_MODE_CUSTOM_PATH     2    // Pre-programmed or user-entered turn sequence
#define NAV_MODE_PID_LINE        3    // Standard PID Line Follower

#define DEFAULT_NAV_MODE         NAV_MODE_AUTONOMOUS_LSRB
#define MAX_PATH_STEPS           64

// Default route from Rulebook Map
#define DEFAULT_RULEBOOK_ROUTE   "RRLLRLSSLLRRLL"

// ============================================================================
//  Default Parameters
// ============================================================================
#define DEFAULT_KP              25.0f
#define DEFAULT_KI              0.1f
#define DEFAULT_KD              15.0f

#define DEFAULT_BASE_SPEED      150
#define DEFAULT_MAX_SPEED       255
#define DEFAULT_MIN_SPEED       0

#define DEFAULT_RAW_MIN         0
#define DEFAULT_RAW_MAX         4095

// Scaling factor for PID calculation: correction = (P + I + D) / PID_SCALE_DIVISOR
#define PID_SCALE_DIVISOR       100.0f
#define PID_ANTI_WINDUP_LIMIT   10000.0f

// ============================================================================
//  Timing & Maze Navigation Configuration
// ============================================================================
#define LOOP_RATE_HZ            100    // Main PID loop frequency (10ms)
#define MOTOR_SAFETY_TIMEOUT_MS 500    // Stop motors if no sensor updates for >500ms
#define CALIBRATION_DURATION_MS 10000  // 10 seconds calibration sweep

#define JUNCTION_INCH_TIME_MS   120    // Inching time to align wheels with junction
#define TURN_SPEED              140    // PWM speed during pivot turns and U-turns
#define TURN_TIMEOUT_MS         2000   // Timeout for in-place turns
#define END_ZONE_INCH_MS        220    // Inching duration to confirm Zone B large black square
#define CHECKPOINT_DEBOUNCE_MS  1000   // Minimum time between checkpoint triggers
#define LINE_LOST_TIMEOUT_MS    1500   // Search timeout when line is lost
