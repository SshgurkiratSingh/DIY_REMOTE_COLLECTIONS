#include "Receiver.h"

Receiver::Receiver()
{
    currentData.failsafe = true;
}

void Receiver::begin()
{
    Serial.begin(115200); // Shared for IBus (RX) and Debug (TX).
                          // Note: IBus is 115200 baud by default
    ibus.begin(Serial);   // Attached to Hardware Serial
}

void Receiver::update()
{
    ibus.loop();

    if (!isConnected())
    {
        currentData.failsafe = true;
        currentData.ch1 = 0;
        currentData.ch2 = 0;
        currentData.speedScale = 0.0;
        currentData.mode = 0; // default safe mode
        return;
    }

    currentData.failsafe = false;

    int rcCh1 = readChannel(0, -255, 255, 0);
    int rcCh2 = readChannel(1, -255, 255, 0);

    // Apply deadband
    if (abs(rcCh1) < DEADBAND)
        rcCh1 = 0;
    if (abs(rcCh2) < DEADBAND)
        rcCh2 = 0;

    int rcCh3 = readChannel(2, 0, 100, 0);       // Raw 0-100 speed scale
    bool rcCh4 = readChannel(3, 0, 100, 0) > 50; // Burst mode

    // Compute final speed scale based on Burst
    if (rcCh4)
    {
        currentData.speedScale = 1.0;
    }
    else
    {
        currentData.speedScale = rcCh3 / 100.0;
    }

    // Multiply by speedScale immediately inside the receiver output
    currentData.ch1 = rcCh1 * currentData.speedScale;
    currentData.ch2 = rcCh2 * currentData.speedScale;

    // Ch5 is Drive Mode: -100, 0, 100
    int rawCh5 = readChannel(4, -100, 100, 0);
    if (rawCh5 < -50)
        currentData.mode = -100;
    else if (rawCh5 > 50)
        currentData.mode = 100;
    else
        currentData.mode = 0;

    // Ch6 is Invert X logic
    currentData.invertX = readChannel(5, 0, 100, 0) > 50;
    if (currentData.invertX)
    {
        currentData.ch1 = -currentData.ch1;
    }
}

bool Receiver::isConnected()
{
    // If we haven't seen a packet in 2 seconds, we consider ourselves disconnected.
    // We can just rely on the readChannel timeout logic (returns default).
    // IBusBM normally handles timeouts by returning default channel value (like 1500). Wait
    // Since readChannel gives defaultValue if less than 100.
    // Actually, IBusBM return 0 for channels if disconnected or not received.

    // Checking channel 1
    uint16_t ch = ibus.readChannel(0);
    if (ch < 100)
        return false;
    return true;
}

int Receiver::readChannel(byte channelInput, int minLimit, int maxLimit, int defaultValue)
{
    uint16_t ch = ibus.readChannel(channelInput);
    if (ch < 100)
        return defaultValue;
    return map(ch, 1000, 2000, minLimit, maxLimit);
}

ReceiverData Receiver::getData()
{
    return currentData;
}
