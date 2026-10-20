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

// Analog IR Sensors (ESP32 ADC1 channels - safe with active WiFi)
#define SENSOR_LEFT_PIN         34   // ADC1_CH6
#define SENSOR_CENTER_PIN       35   // ADC1_CH7
#define SENSOR_RIGHT_PIN        36   // ADC1_CH0

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
