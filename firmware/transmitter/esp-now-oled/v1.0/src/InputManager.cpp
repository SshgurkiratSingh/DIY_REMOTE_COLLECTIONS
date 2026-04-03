#include "InputManager.h"

volatile long InputManager::encoderPos = 0;
volatile uint8_t InputManager::encLastState = 0;

void IRAM_ATTR InputManager::isrEncoder()
{
    uint8_t a = digitalRead(PIN_ENC_CLK);
    uint8_t b = digitalRead(PIN_ENC_DT);
    uint8_t state = (a << 1) | b;

    // Simple state-machine based quadrature decoding could be added here
    if (a != encLastState)
    {
        if (digitalRead(PIN_ENC_DT) != a)
        {
            encoderPos++;
        }
        else
        {
            encoderPos--;
        }
    }
    encLastState = a;
}

void InputManager::begin()
{
    // Analog (ADC1) config
    analogReadResolution(12);

    // Digital configs
    pinMode(PIN_TOGGLE_1, INPUT_PULLUP);
    pinMode(PIN_TOGGLE_2, INPUT_PULLUP);
    pinMode(PIN_PUSH_1, INPUT_PULLUP);
    pinMode(PIN_PUSH_2, INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);

    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
    pinMode(PIN_ENC_DT, INPUT_PULLUP);

    encLastState = digitalRead(PIN_ENC_CLK);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), isrEncoder, CHANGE);
}

void InputManager::readInputs(struct_message *data, const SystemConfig &config)
{
    uint16_t rawX = analogRead(PIN_JOY_VRX);
    uint16_t rawY = analogRead(PIN_JOY_VRY);

    // Apply Deadzone
    if (abs((int)rawX - (int)config.centerX) <= config.deadzone)
    {
        rawX = config.centerX;
    }
    if (abs((int)rawY - (int)config.centerY) <= config.deadzone)
    {
        rawY = config.centerY;
    }

    // Apply Inversion
    if (config.invertX)
        rawX = 4095 - rawX;
    if (config.invertY)
        rawY = 4095 - rawY;

    data->joyX = rawX;
    data->joyY = rawY;
    data->potValue = analogRead(PIN_POT);

    data->toggle1 = !digitalRead(PIN_TOGGLE_1);
    data->toggle2 = !digitalRead(PIN_TOGGLE_2);
    data->push1 = !digitalRead(PIN_PUSH_1);
    data->push2 = !digitalRead(PIN_PUSH_2);
    data->encSw = !digitalRead(PIN_ENC_SW);

    // Safely read volatile variable
    noInterrupts();
    data->encoderPos = encoderPos;
    interrupts();
}

long InputManager::getEncoderPosition()
{
    long pos;
    noInterrupts();
    pos = encoderPos;
    interrupts();
    return pos;
}
