#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <esp_now.h>
#include <WiFi.h>
#include "Config.h"

class NetworkManager
{
public:
    bool begin(const SystemConfig &config);
    bool sendData(struct_message *data);
    bool getLastStatus() const { return lastSendSuccess; }
    void updateTarget(const uint8_t *mac);
    rx_message getLastRxData() const { return lastRxData; }

private:
    static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len);
    static bool lastSendSuccess;
    static rx_message lastRxData;
    esp_now_peer_info_t peerInfo;
    uint8_t currentTarget[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
};

#endif
