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
};

#endif
