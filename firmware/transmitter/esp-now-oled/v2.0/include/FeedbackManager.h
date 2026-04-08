#ifndef FEEDBACK_MANAGER_H
#define FEEDBACK_MANAGER_H

#include <Arduino.h>
#include "Config.h"

enum class FeedbackEvent {
    NONE,
    STARTUP,
    BUTTON_PRESS,
    CONNECTION_RECOVERED,
    CONNECTION_LOST
};

class FeedbackManager {
public:
    FeedbackManager();
    void begin();
    void trigger(FeedbackEvent event);
    void update(const SystemConfig &config);
    void stopAll();
    void setContinuousVibrator(bool on, const SystemConfig &config);

private:
    bool _playing;
    FeedbackEvent _currentEvent;
    unsigned long _eventStartTime;
    unsigned long _lastUpdate;
    int _step;
    
    // Config snapshot for current event
    bool _vibEnabled;
    bool _buzEnabled;

    void processStartup();
    void processButtonPress();
    void processConnectionRecovered();
    void processConnectionLost();

    void setBuzzer(uint32_t freq);
    void setVibrator(bool on);
};

extern FeedbackManager feedbackManager;

#endif
