#include "FeedbackManager.h"

// Define PWM constants for buzzer
#define BUZZER_CHANNEL 0
#define BUZZER_RESOLUTION 8

FeedbackManager feedbackManager;

FeedbackManager::FeedbackManager() {
    _playing = false;
    _currentEvent = FeedbackEvent::NONE;
    _eventStartTime = 0;
    _lastUpdate = 0;
    _step = 0;
    _vibEnabled = false;
    _buzEnabled = false;
}

void FeedbackManager::begin() {
    // Configure Pins
    pinMode(PIN_LED_YELLOW, OUTPUT); // Virbrator / Motor
    digitalWrite(PIN_LED_YELLOW, LOW);

    pinMode(PIN_LED_GREEN, OUTPUT); // Buzzer
    digitalWrite(PIN_LED_GREEN, LOW);
    
    // Setup PWM channel but don't attach yet until needed
    ledcSetup(BUZZER_CHANNEL, 2000, BUZZER_RESOLUTION);
}

void FeedbackManager::trigger(FeedbackEvent event) {
    _currentEvent = event;
    _playing = true;
    _eventStartTime = millis();
    _lastUpdate = _eventStartTime;
    _step = 0;
}

void FeedbackManager::update(const SystemConfig &config) {
    if (!_playing || config.outType != OUT_VIB_BUZZ) {
        stopAll();
        return;
    }

    // Refresh config for the current playback
    _vibEnabled = (config.feedbackMode == FB_VIB_ONLY || config.feedbackMode == FB_VIB_BUZ);
    _buzEnabled = (config.feedbackMode == FB_BUZ_ONLY || config.feedbackMode == FB_VIB_BUZ);

    switch (_currentEvent) {
        case FeedbackEvent::STARTUP:
            processStartup();
            break;
        case FeedbackEvent::BUTTON_PRESS:
            processButtonPress();
            break;
        case FeedbackEvent::CONNECTION_RECOVERED:
            processConnectionRecovered();
            break;
        case FeedbackEvent::CONNECTION_LOST:
            processConnectionLost();
            break;
        default:
            stopAll();
            break;
    }
}

void FeedbackManager::stopAll() {
    _playing = false;
    _currentEvent = FeedbackEvent::NONE;
    setBuzzer(0);
    setVibrator(false);
}


void FeedbackManager::setContinuousVibrator(bool on, const SystemConfig &config) {
    if (config.outType != OUT_VIB_BUZZ) return;
    _vibEnabled = (config.feedbackMode == FB_VIB_ONLY || config.feedbackMode == FB_VIB_BUZ);
    // Don't interrupt other events with stop calls unless we have to, but we just set the pin directly.
    setVibrator(on);
}

void FeedbackManager::setBuzzer(uint32_t freq) {
    if (!_buzEnabled || freq == 0) {
        ledcDetachPin(PIN_LED_GREEN);
        pinMode(PIN_LED_GREEN, OUTPUT);
        digitalWrite(PIN_LED_GREEN, LOW);
    } else {
        ledcAttachPin(PIN_LED_GREEN, BUZZER_CHANNEL);
        ledcWriteTone(BUZZER_CHANNEL, freq);
    }
}

void FeedbackManager::setVibrator(bool on) {
    if (!_vibEnabled || !on) {
        digitalWrite(PIN_LED_YELLOW, LOW);
    } else {
        digitalWrite(PIN_LED_YELLOW, HIGH);
    }
}

void FeedbackManager::processStartup() {
    unsigned long t = millis() - _eventStartTime;
    
    if (_step == 0) { // 0ms
        setBuzzer(523); // C4
        setVibrator(true);
        _step++;
    } else if (_step == 1 && t > 100) {
        setBuzzer(659); // E4
        _step++;
    } else if (_step == 2 && t > 200) {
        setBuzzer(783); // G4
        _step++;
    } else if (_step == 3 && t > 300) {
        stopAll();
    }
}

void FeedbackManager::processButtonPress() {
    unsigned long t = millis() - _eventStartTime;
    
    if (_step == 0) {
        setBuzzer(2000);
        setVibrator(true);
        _step++;
    } else if (_step == 1 && t > 30) { // 30ms click pulse
        stopAll();
    }
}

void FeedbackManager::processConnectionRecovered() {
    unsigned long t = millis() - _eventStartTime;

    if (_step == 0) {
        setBuzzer(800);
        setVibrator(true);
        _step++;
    } else if (_step == 1 && t > 100) {
        setBuzzer(0);
        setVibrator(false);
        _step++;
    } else if (_step == 2 && t > 150) {
        setBuzzer(1000);
        setVibrator(true);
        _step++;
    } else if (_step == 3 && t > 300) {
        stopAll();
    }
}

void FeedbackManager::processConnectionLost() {
    unsigned long t = millis() - _eventStartTime;
    // 3 Pulses: on 200, off 100
    
    if (_step == 0) {
        setBuzzer(600);
        setVibrator(true);
        _step++;
    } else if (_step == 1 && t > 200) { // off
        setBuzzer(0);
        setVibrator(false);
        _step++;
    } else if (_step == 2 && t > 300) { // on
        setBuzzer(450);
        setVibrator(true);
        _step++;
    } else if (_step == 3 && t > 500) { // off
        setBuzzer(0);
        setVibrator(false);
        _step++;
    } else if (_step == 4 && t > 600) { // on
        setBuzzer(300);
        setVibrator(true);
        _step++;
    } else if (_step == 5 && t > 800) {
        stopAll();
    }
}
