#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "Config.h"

class InputManager
{
public:
    void begin();
    void readInputs(struct_message *data, const SystemConfig &config);
    long getEncoderPosition();

private:
    static volatile long encoderPos;
    static volatile uint8_t encLastState;
    static void IRAM_ATTR isrEncoder();

    // ADC filtering with EMA (exponential moving average)
    uint16_t filteredX = 2048;
    uint16_t filteredY = 2048;
    uint16_t filteredPot = 0;

    // Hysteresis for deadzone (prevents jitter)
    bool inDeadzoneX = false;
    bool inDeadzoneY = false;

    static const float EMA_ALPHA;            // Smoothing factor (0.1 = 10% new value)
    static const uint16_t HYSTERESIS_MARGIN; // Hysteresis margin in units
};

#endif
