#include "InputManager.h"
#include "driver/gpio.h"

volatile long InputManager::encoderPos = 0;
volatile uint8_t InputManager::encLastState = 0;

// EMA filter coefficient (0.35 = 35% new value + 65% old value) for faster response
const float InputManager::EMA_ALPHA = 0.35f;

// Smaller margin exits deadzone sooner while still suppressing ADC jitter
const uint16_t InputManager::HYSTERESIS_MARGIN = 3;

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
    // Analog (ADC1) config - 12-bit resolution
    analogReadResolution(12);

    // ADC attenuation for better range on 0-3.3V inputs
    // (More stable readings with reduced noise in mid-range)
    analogSetAttenuation(ADC_11db);

    // Digital configs
    pinMode(PIN_TOGGLE_1, INPUT_PULLUP);
    pinMode(PIN_TOGGLE_2, INPUT_PULLUP);
    pinMode(PIN_PUSH_1, INPUT_PULLUP);
    pinMode(PIN_PUSH_2, INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);

    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
    pinMode(PIN_ENC_DT, INPUT_PULLUP);

    // Force pull-up-only on toggle lines to avoid mixed pull states.
    gpio_set_pull_mode((gpio_num_t)PIN_TOGGLE_1, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)PIN_TOGGLE_2, GPIO_PULLUP_ONLY);

    encLastState = digitalRead(PIN_ENC_CLK);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), isrEncoder, CHANGE);

    // Initialize filtered values with initial reads
    filteredX = analogRead(PIN_JOY_VRX);
    filteredY = analogRead(PIN_JOY_VRY);
    filteredPot = analogRead(PIN_POT);
}

void InputManager::readInputs(struct_message *data, const SystemConfig &config)
{
    // Read raw ADC values
    uint16_t rawX = analogRead(PIN_JOY_VRX);
    uint16_t rawY = analogRead(PIN_JOY_VRY);
    uint16_t rawPot = analogRead(PIN_POT);

    // Apply Exponential Moving Average (EMA) filter to reduce noise
    // This significantly reduces the ~10 unit jitter on ESP32 ADC
    filteredX = (uint16_t)(EMA_ALPHA * rawX + (1.0f - EMA_ALPHA) * filteredX);
    filteredY = (uint16_t)(EMA_ALPHA * rawY + (1.0f - EMA_ALPHA) * filteredY);
    filteredPot = (uint16_t)(EMA_ALPHA * rawPot + (1.0f - EMA_ALPHA) * filteredPot);

    // Apply Deadzone with Hysteresis to prevent jitter
    // Hysteresis: only exit deadzone if distance exceeds deadzone + margin
    int diffX = (int)filteredX - (int)config.centerX;
    int diffY = (int)filteredY - (int)config.centerY;

    // X-axis hysteresis
    if (inDeadzoneX)
    {
        // In deadzone - only exit if we exceed deadzone + margin
        if (abs(diffX) > config.deadzone + HYSTERESIS_MARGIN)
        {
            inDeadzoneX = false; // Exit deadzone
        }
    }
    else
    {
        // Outside deadzone - enter if within deadzone bounds
        if (abs(diffX) <= config.deadzone)
        {
            inDeadzoneX = true; // Enter deadzone
        }
    }

    // Y-axis hysteresis
    if (inDeadzoneY)
    {
        // In deadzone - only exit if we exceed deadzone + margin
        if (abs(diffY) > config.deadzone + HYSTERESIS_MARGIN)
        {
            inDeadzoneY = false; // Exit deadzone
        }
    }
    else
    {
        // Outside deadzone - enter if within deadzone bounds
        if (abs(diffY) <= config.deadzone)
        {
            inDeadzoneY = true; // Enter deadzone
        }
    }

    // Set output values based on deadzone state
    uint16_t outputX = inDeadzoneX ? config.centerX : filteredX;
    uint16_t outputY = inDeadzoneY ? config.centerY : filteredY;

    // Apply Inversion
    if (config.invertX)
        outputX = 4095 - outputX;
    if (config.invertY)
        outputY = 4095 - outputY;

    data->joyX = outputX;
    data->joyY = outputY;
    data->potValue = filteredPot;

    data->toggle1 = !digitalRead(PIN_TOGGLE_1);
    data->toggle2 = !digitalRead(PIN_TOGGLE_2);
    data->push1 = !digitalRead(PIN_PUSH_1);
    data->push2 = !digitalRead(PIN_PUSH_2);
}

long InputManager::getEncoderPosition()
{
    long pos;
    noInterrupts();
    pos = encoderPos;
    interrupts();
    return pos;
}
