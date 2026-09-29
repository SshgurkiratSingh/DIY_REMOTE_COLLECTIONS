#include "Network.h"

Network* Network::instance = nullptr;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void Network::onDataRecvStatic(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (instance && info && info->src_addr) {
        instance->handleDataRecv(info->src_addr, data, len);
    }
}
#else
void Network::onDataRecvStatic(const uint8_t *mac, const uint8_t *data, int len) {
    if (instance) {
        instance->handleDataRecv(mac, data, len);
    }
}
#endif

Network::Network() : 
    lastRecvTime(0), lastFeedbackTime(0), hasSender(false), lastSeenLedMode(-1) {
    instance = this;
    memset((void*)&incomingData, 0, sizeof(incomingData));
    incomingData.joyX = JOY_CENTER;
    incomingData.joyY = JOY_CENTER;
}

bool Network::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.printf("Receiver MAC: %s\n", WiFi.macAddress().c_str());

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init FAILED!");
        return false;
    }
    
    // Register callback based on ESP32 Core version
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    esp_now_register_recv_cb((esp_now_recv_cb_t)Network::onDataRecvStatic);
#else
    esp_now_register_recv_cb(Network::onDataRecvStatic);
#endif
    
    return true;
}

bool Network::isConnected() const {
    return (millis() - lastRecvTime <= SIGNAL_TIMEOUT_MS);
}

bool Network::hasNewLedMode() {
    if (incomingData.addrLedMode != lastSeenLedMode) {
        lastSeenLedMode = incomingData.addrLedMode;
        return true;
    }
    return false;
}

void Network::handleDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if ((size_t)len != sizeof(struct_message)) {
        // Size mismatch, ignore
        return;
    }

    struct_message tmp;
    memcpy(&tmp, data, sizeof(tmp));

    if (EXPECTED_VERIFY_KEY != 0 && tmp.verifyKey != EXPECTED_VERIFY_KEY) {
        return; // Unauthorized
    }

    // Safe copy (ISR context)
    memcpy((void*)&incomingData, &tmp, sizeof(tmp));
    lastRecvTime = millis();

    // Auto-pair on first packet
    if (!hasSender) {
        memcpy(senderMac, mac, 6);
        hasSender = true;

        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, senderMac, 6);
        peer.channel = 0;
        peer.encrypt = false;
        if (!esp_now_is_peer_exist(senderMac)) {
            esp_now_add_peer(&peer);
        }

        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        Serial.printf("Paired with: %s\n", buf);
    }
}

void Network::update(uint8_t currentLedMode, bool motorSyncEnabled) {
    if (!hasSender) return;
    
    unsigned long now = millis();
    if (now - lastFeedbackTime < FEEDBACK_INTERVAL_MS) return;
    lastFeedbackTime = now;

    feedbackData.data1 = currentLedMode;
    feedbackData.data2 = motorSyncEnabled ? 1 : 0;
    feedbackData.data3 = (uint8_t)map(analogRead(BATT_ADC_PIN), 0, 4095, 0, 255);
    
    esp_now_send(senderMac, (uint8_t*)&feedbackData, sizeof(feedbackData));
}
