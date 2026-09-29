#pragma once

#include <Arduino.h>

// ============================================================================
//  Pin Definitions
// ============================================================================

// L298N Motor Driver
#define ENA_PIN        25
#define IN1_PIN        26
#define IN2_PIN        27
#define ENB_PIN        14
#define IN3_PIN        12
#define IN4_PIN        13

// WS2812B LED Strips (Left eye / Right eye)
#define LED_PIN_LEFT   15
#define LED_PIN_RIGHT  16
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB

// Status LED (onboard)
#define STATUS_LED     2

// Battery voltage divider (optional, ADC1)
#define BATT_ADC_PIN   34

// ============================================================================
//  Configuration
// ============================================================================

// Motor PWM (ESP32 LEDC)
#define PWM_FREQ         5000
#define PWM_RES_BITS     8
#define PWM_CHANNEL_A    0
#define PWM_CHANNEL_B    1

// Joystick
#define JOY_MAX          4095
#define JOY_CENTER       2048
#define JOY_DEADZONE_RAW 150

// Speed ramping (units per loop tick at ~50 Hz update rate)
#define RAMP_STEP        12

// Safety
#define SIGNAL_TIMEOUT_MS    1000
#define FEEDBACK_INTERVAL_MS 500

// LED
#define NUM_LEDS          10
#define MIN_BRIGHTNESS    10
#define DEFAULT_BRIGHTNESS 150
#define TOTAL_LED_MODES   10

// Packet auth (0 = accept any transmitter)
#define EXPECTED_VERIFY_KEY 0

// ============================================================================
//  Data Structures (MUST match transmitter v2.0 exactly)
// ============================================================================

typedef struct __attribute__((packed)) struct_message {
    uint16_t joyX;
    uint16_t joyY;
    uint16_t potValue;
    bool     toggle1;
    bool     toggle2;
    bool     push1;
    bool     push2;
    uint8_t  verifyKey;
    uint8_t  addrLedMode;
} struct_message;

typedef struct __attribute__((packed)) rx_message {
    uint8_t data1;   // active LED mode
    uint8_t data2;   // motor-sync flag
    uint8_t data3;   // battery (0-255)
} rx_message;
