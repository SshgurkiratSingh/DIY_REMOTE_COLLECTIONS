#include "FeedbackManager.h"
#include "NetworkManager.h"

bool NetworkManager::lastSendSuccess = false;
int NetworkManager::consecutiveTxFails = 0;
bool NetworkManager::isConnected = false;
rx_message NetworkManager::lastRxData = {0, 0, 0};

// Default broadcast just to stay safe until a target is picked
const uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void NetworkManager::OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    lastSendSuccess = (status == ESP_NOW_SEND_SUCCESS);
    if (lastSendSuccess) {
        if (consecutiveTxFails >= 5 || !isConnected) {
            isConnected = true;
            feedbackManager.trigger(FeedbackEvent::CONNECTION_RECOVERED);
        }
        consecutiveTxFails = 0;
    } else {
        consecutiveTxFails++;
        if (consecutiveTxFails == 5) {
            isConnected = false;
            feedbackManager.trigger(FeedbackEvent::CONNECTION_LOST);
        }
    }
}

void NetworkManager::OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    if (len == sizeof(rx_message))
    {
        memcpy(&lastRxData, data, sizeof(rx_message));
    }
}

bool NetworkManager::begin(const SystemConfig &config)
{
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        return false;
    }

    esp_now_register_send_cb(OnDataSent);

    // Conditionally enable half-duplex Rx
    if (config.rxEnabled)
    {
        esp_now_register_recv_cb(OnDataRecv);
    }

    memcpy(currentTarget, broadcastAddress, 6);
    memcpy(peerInfo.peer_addr, currentTarget, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        return false;
    }

    return true;
}

void NetworkManager::updateTarget(const uint8_t *mac)
{
    // Remove old peer
    esp_now_del_peer(currentTarget);

    // Setup new peer
    memcpy(currentTarget, mac, 6);
    memcpy(peerInfo.peer_addr, currentTarget, 6);

    // Add new peer
    esp_now_add_peer(&peerInfo);
}

bool NetworkManager::sendData(struct_message *data)
{
    // Transmit to the dynamically set target
    esp_err_t result = esp_now_send(currentTarget, (uint8_t *)data, sizeof(struct_message));
    return result == ESP_OK;
}
