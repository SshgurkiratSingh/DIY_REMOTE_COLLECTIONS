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
// Balanced Symmetric Layout (3 physical regions x 3 sensors = 9 sensors):
//   LEFT SIDE (3)     |    CENTER (3)     |    RIGHT SIDE (3)
//   S0    S1    S2    |   S3    S4    S5    |   S6    S7    S8
//  [FL]  [ML]  [IL]   |  [CL]  [CM]  [CR]   |  [IR]  [MR]  [FR]
//  ADC2  ADC1  ADC1   |  ADC1  ADC2  ADC1   |  ADC1  ADC1  ADC2
//
// WiFi Calibration Active (ADC1):
//   Left side : 2 sensors (S1, S2)
//   Center    : 2 sensors (S3, S5)
//   Right side: 2 sensors (S6, S7)
//   --> Exactly 2 IR sensors on Left, Center, and Right during WiFi on!
//
// Running Mode Only (ADC2, WiFi OFF):
//   Left side : S0 (GPIO 4, Far Left)
//   Center    : S4 (GPIO 2, Center Mid, shares LED)
//   Right side: S8 (GPIO 15, Far Right)
//
#define NUM_SENSORS             9
#define ADC1_CALIB_COUNT        6    // Total sensors calibrated during WiFi on (2 left, 2 center, 2 right)
#define SENSORS_PER_REGION      3    // 3 Left, 3 Center, 3 Right
#define CALIB_PER_REGION        2    // 2 Left, 2 Center, 2 Right active during WiFi calibration

// Pin mappings:
#define SENSOR_S0_PIN           4    // Left Side  : Far Left     (ADC2_CH0, running only)
#define SENSOR_S1_PIN           39   // Left Side  : Mid Left     (ADC1_CH3, WiFi-safe)
#define SENSOR_S2_PIN           36   // Left Side  : Inner Left   (ADC1_CH0, WiFi-safe)

#define SENSOR_S3_PIN           35   // Center     : Center-Left  (ADC1_CH7, WiFi-safe)
#define SENSOR_S4_PIN           2    // Center     : Center-Mid   (ADC2_CH2, running only, shares LED)
#define SENSOR_S5_PIN           34   // Center     : Center-Right (ADC1_CH6, WiFi-safe)

#define SENSOR_S6_PIN           33   // Right Side : Inner Right  (ADC1_CH5, WiFi-safe)
#define SENSOR_S7_PIN           32   // Right Side : Mid Right    (ADC1_CH4, WiFi-safe)
#define SENSOR_S8_PIN           15   // Right Side : Far Right    (ADC2_CH3, running only)

// Symmetric weights: Center (S4) = 0, range [-4000 .. +4000]
#define SENSOR_WEIGHTS          {-4000, -3000, -2000, -1000, 0, 1000, 2000, 3000, 4000}

// Flag whether sensor[i] is ADC1 (readable with WiFi on)
#define SENSOR_IS_ADC1          {false, true, true, true, false, true, true, true, false}

// Aliases for junction/turn detection
#define SENSOR_LEFT_PIN         SENSOR_S1_PIN
#define SENSOR_CENTER_PIN       SENSOR_S3_PIN
#define SENSOR_RIGHT_PIN        SENSOR_S7_PIN

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
