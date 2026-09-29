#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "Config.h"

class Network {
public:
    Network();
    
    // Initializes WiFi and ESP-NOW
    bool begin();
    
    // Call in loop to handle telemetry feedback
    void update(uint8_t currentLedMode, bool motorSyncEnabled);
    
    // Returns true if a valid packet was received recently
    bool isConnected() const;
    
    // Returns the latest received data
    const struct_message& getData() const { return incomingData; }
    
    // Checks if the packet contains a new mode and consumes it (returning true once)
    bool hasNewLedMode();

private:
    static void onDataRecvStatic(const uint8_t *mac, const uint8_t *data, int len);
    void handleDataRecv(const uint8_t *mac, const uint8_t *data, int len);

    struct_message incomingData;
    rx_message feedbackData;
    
    unsigned long lastRecvTime;
    unsigned long lastFeedbackTime;
    
    uint8_t senderMac[6];
    bool hasSender;
    
    int lastSeenLedMode; // To track when the mode actually changes

    // Singleton instance pointer for the static callback
    static Network* instance;
};
