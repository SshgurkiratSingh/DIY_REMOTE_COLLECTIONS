#pragma once

#include <Arduino.h>

typedef struct struct_message
{
    uint16_t joyX;
    uint16_t joyY;
    uint16_t potValue;
    bool toggle1;
    bool toggle2;
    bool push1;
    bool push2;
} struct_message;

typedef struct struct_reply
{
    uint8_t sensorValue;
    uint8_t counter;
    uint8_t flags;
} struct_reply;

struct MotorDriver
{
    int rpwmPin;
    int lpwmPin;
    int lenPin;
    int renPin;
    int chanForward;
    int chanReverse;
};

struct ImuState
{
    float yawRad;
    float pitchDeg;
    float rollDeg;
    float forwardAccel;
};

struct DriveOutput
{
    int targetLeft;
    int targetRight;
    bool enableRamp;
    bool burstActive;
    float rampRate;
    bool mode2Headless;
    bool mode3FullAssist;
};
