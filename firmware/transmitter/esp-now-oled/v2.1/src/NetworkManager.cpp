#include "FeedbackManager.h"
#include "NetworkManager.h"

bool NetworkManager::lastSendSuccess = false;
int NetworkManager::consecutiveTxFails = 0;
bool NetworkManager::isConnected = false;
rx_message NetworkManager::lastRxData = {0, 0, 0};

const uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

namespace
{
    bool isZeroMac(const uint8_t *mac)
    {
        for (int i = 0; i < 6; i++)
        {
            if (mac[i] != 0x00)
            {
                return false;
            }
        }
        return true;
    }
} // namespace

void NetworkManager::OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    lastSendSuccess = (status == ESP_NOW_SEND_SUCCESS);
    if (lastSendSuccess)
    {
        if (consecutiveTxFails >= 5 || !isConnected)
        {
            isConnected = true;
            feedbackManager.trigger(FeedbackEvent::CONNECTION_RECOVERED);
        }
        consecutiveTxFails = 0;
    }
    else
    {
        consecutiveTxFails++;
        if (consecutiveTxFails == 5)
        {
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

    memset(&peerInfo, 0, sizeof(peerInfo));
    hasActiveTarget = false;
    memset(currentTarget, 0, sizeof(currentTarget));

    // Default to broadcast peer as in v1.3 config
    updateTarget(broadcastAddress);

    return true;
}

bool NetworkManager::updateTarget(const uint8_t *mac)
{
    if (mac == nullptr || isZeroMac(mac))
    {
        return false;
    }

    if (hasActiveTarget)
    {
        esp_now_del_peer(currentTarget);
        hasActiveTarget = false;
    }

    esp_now_peer_info_t nextPeer = {};
    memcpy(nextPeer.peer_addr, mac, 6);
    nextPeer.channel = 0;
    nextPeer.encrypt = false;

    if (esp_now_add_peer(&nextPeer) != ESP_OK)
    {
        return false;
    }

    memcpy(currentTarget, mac, 6);
    peerInfo = nextPeer;
    hasActiveTarget = true;
    return true;
}

bool NetworkManager::sendData(struct_message *data)
{
    if (!hasActiveTarget)
    {
        return false;
    }

    esp_err_t result = esp_now_send(currentTarget, (uint8_t *)data, sizeof(struct_message));
    return result == ESP_OK;
}
