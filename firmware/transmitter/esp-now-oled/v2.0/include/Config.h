#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Analog Inputs (ADC1) ---
#define PIN_JOY_VRY 33
#define PIN_POT 32
#define PIN_JOY_VRX 35

// --- Digital Inputs ---
#define PIN_ENC_CLK 27
#define PIN_ENC_DT 14
#define PIN_ENC_SW 13
#define PIN_TOGGLE_1 26
#define PIN_TOGGLE_2 25
#define PIN_PUSH_1 19
#define PIN_PUSH_2 15

// --- Outputs ---
#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22
#define PIN_LED_YELLOW 23
#define PIN_LED_GREEN 18

// --- Display Config ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64 // Or 64 based on your actual display
#define OLED_ADDR 0x3C

#define FIRMWARE_VERSION "v1.2.0"

enum LedMode
{
    LED_TX_RX = 0,
    LED_LINK_STATE,
    LED_ACTION,
    LED_STEALTH,
    LED_MODE_MAX
};

enum OutType
{
    OUT_LEDS = 0,
    OUT_VIB_BUZZ,
    OUT_TYPE_MAX
};

enum FeedbackMode
{
    FB_SILENT = 0,
    FB_VIB_ONLY,
    FB_BUZ_ONLY,
    FB_VIB_BUZ,
    FB_MODE_MAX
};

// --- Data Packet ---
typedef struct struct_message
{
    uint16_t joyX;
    uint16_t joyY;
    uint16_t potValue;
    bool toggle1;
    bool toggle2;
    bool push1;
    bool push2;
    uint8_t verifyKey;
} struct_message;

// --- Received Data Packet ---
typedef struct rx_message
{
    uint8_t data1;
    uint8_t data2;
    uint8_t data3;
} rx_message;

#define MAX_TARGETS 5

struct TargetNode
{
    uint8_t mac[6];
    char name[16];
    uint8_t verifyKey;
    bool active;
};

// --- System Configuration & NVS ---
struct SystemConfig
{
    uint16_t deadzone;
    uint16_t centerX;
    uint16_t centerY;
    bool invertX;
    bool invertY;
    uint8_t txRateHz;
    bool rxEnabled;
    uint8_t ledMode;
    uint8_t outType;
    uint8_t feedbackMode;
};

// Replace with receiver MAC address
extern const uint8_t broadcastAddress[6];

#endif
