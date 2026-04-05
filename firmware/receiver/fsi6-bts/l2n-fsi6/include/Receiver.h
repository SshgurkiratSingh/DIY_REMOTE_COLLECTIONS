#ifndef RECEIVER_H
#define RECEIVER_H

#include <Arduino.h>
#include <IBusBM.h>

struct ReceiverData
{
    int ch1;          // Steering / Right Tank (Mapped -255 to 255)
    int ch2;          // Throttle / Left Tank (Mapped -255 to 255)
    float speedScale; // Ch3 (0.0 to 1.0)
    bool burstMode;   // Ch4 (>50%)
    int mode;         // Ch5 (-100, 0, 100)
    bool invertX;     // Ch6 (>50%)
    bool failsafe;    // Whether connection is lost
};

class Receiver
{
public:
    Receiver();
    void begin();
    void update();
    ReceiverData getData();

private:
    IBusBM ibus;
    ReceiverData currentData;
    const int DEADBAND = 10;

    int readChannel(byte channelInput, int minLimit, int maxLimit, int defaultValue);
    bool isConnected();
};

#endif // RECEIVER_H
