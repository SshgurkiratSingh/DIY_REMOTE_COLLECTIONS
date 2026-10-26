#pragma once

#include <Arduino.h>

// ============================================================================
//  Hardware Pin Definitions
// ============================================================================

// TB6612FNG Dual H-Bridge Motor Driver Pins
#define MOTOR_PWMA_PIN          12   // D12: PWM speed control for Motor A (Left)
#define MOTOR_AIN1_PIN          4    // D4 : Direction control 1 for Motor A (Left)
#define MOTOR_AIN2_PIN          2    // D2 : Direction control 2 for Motor A (Left)
#define MOTOR_PWMB_PIN          13   // D13: PWM speed control for Motor B (Right)
#define MOTOR_BIN1_PIN          15   // D15: Direction control 1 for Motor B (Right)
#define MOTOR_BIN2_PIN          23   // D23: Direction control 2 for Motor B (Right)
#define MOTOR_STBY_PIN          22   // D22: Standby control (HIGH = enabled, LOW = standby)

// Backward-compatible aliases
#define ENA_PIN                 MOTOR_PWMA_PIN
#define IN1_PIN                 MOTOR_AIN1_PIN
#define IN2_PIN                 MOTOR_AIN2_PIN
#define ENB_PIN                 MOTOR_PWMB_PIN
#define IN3_PIN                 MOTOR_BIN1_PIN
#define IN4_PIN                 MOTOR_BIN2_PIN
#define STBY_PIN                MOTOR_STBY_PIN

// 9-Channel Analog IR Sensor Array
//
// Layout (3 physical regions x 3 sensors = 9 sensors):
//   LEFT SIDE (3)     |    CENTER (3)     |    RIGHT SIDE (3)
//   S0    S1    S2    |   S3    S4    S5    |   S6    S7    S8
//  [FL]  [ML]  [IL]   |  [CL]  [CM]  [CR]   |  [IR]  [MR]  [FR]
//  ADC2  ADC1  ADC1   |  ADC1  ADC1  ADC1   |  ADC1  ADC2  ADC2
//
// WiFi Calibration Active (ADC1):
//   Left side : 2 sensors (S1, S2)
//   Center    : 3 sensors (S3, S4, S5) - Dead-center S4 is ACTIVE on clean ADC1!
//   Right side: 1 sensor  (S6)
//   Total: 6 ADC1 sensors active during WiFi calibration.
//
// Running Mode Only (ADC2, WiFi OFF):
//   Left side : S0 (GPIO 25, Far Left)
//   Right side: S7 (GPIO 26, Mid Right), S8 (GPIO 27, Far Right)
//
// Contiguous left rail pins on ESP32 DevKit: VP, VN, 34, 35, 33, 32, 25, 26, 27
#define NUM_SENSORS             9
#define ADC1_CALIB_COUNT        6    // Total sensors calibrated during WiFi on (2 left, 3 center, 1 right)
#define SENSORS_PER_REGION      3    // 3 Left, 3 Center, 3 Right

// Pin mappings:
#define SENSOR_S0_PIN           25   // Left Side  : Far Left     (D25, ADC2_CH8, running only)
#define SENSOR_S1_PIN           39   // Left Side  : Mid Left     (VN,  ADC1_CH3, WiFi-safe)
#define SENSOR_S2_PIN           36   // Left Side  : Inner Left   (VP,  ADC1_CH0, WiFi-safe)

#define SENSOR_S3_PIN           35   // Center     : Center-Left  (D35, ADC1_CH7, WiFi-safe)
#define SENSOR_S4_PIN           34   // Center     : Center-Mid   (D34, ADC1_CH6, WiFi-safe, dead center!)
#define SENSOR_S5_PIN           33   // Center     : Center-Right (D33, ADC1_CH5, WiFi-safe)

#define SENSOR_S6_PIN           32   // Right Side : Inner Right  (D32, ADC1_CH4, WiFi-safe)
#define SENSOR_S7_PIN           26   // Right Side : Mid Right    (D26, ADC2_CH9, running only)
#define SENSOR_S8_PIN           27   // Right Side : Far Right    (D27, ADC2_CH7, running only)

// Symmetric weights: Center (S4) = 0, range [-4000 .. +4000]
#define SENSOR_WEIGHTS          {-4000, -3000, -2000, -1000, 0, 1000, 2000, 3000, 4000}

// Flag whether sensor[i] is ADC1 (readable with WiFi on)
#define SENSOR_IS_ADC1          {false, true, true, true, true, true, true, false, false}

// Aliases for junction/turn detection
#define SENSOR_LEFT_PIN         SENSOR_S1_PIN
#define SENSOR_CENTER_PIN       SENSOR_S4_PIN
#define SENSOR_RIGHT_PIN        SENSOR_S6_PIN

// Indicators & Buttons
#define STATUS_LED_PIN          2    // Built-in status LED (shares AIN2, safe with STBY low)
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
