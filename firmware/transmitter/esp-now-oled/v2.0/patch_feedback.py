with open('src/FeedbackManager.cpp', 'r') as f:
    fc = f.read()

old_setbuzz = """void FeedbackManager::setBuzzer(uint32_t freq) {
    if (!_buzEnabled || freq == 0) {
        ledcWriteTone(BUZZER_CHANNEL, 0);
    } else {
        ledcWriteTone(BUZZER_CHANNEL, freq);
    }
}"""

new_setbuzz = """void FeedbackManager::setBuzzer(uint32_t freq) {
    if (!_buzEnabled || freq == 0) {
        ledcDetachPin(PIN_LED_GREEN);
        pinMode(PIN_LED_GREEN, OUTPUT);
        digitalWrite(PIN_LED_GREEN, LOW);
    } else {
        ledcAttachPin(PIN_LED_GREEN, BUZZER_CHANNEL);
        ledcWriteTone(BUZZER_CHANNEL, freq);
    }
}"""

fc = fc.replace(old_setbuzz, new_setbuzz)

old_begin = """    pinMode(PIN_LED_GREEN, OUTPUT); // Buzzer
    digitalWrite(PIN_LED_GREEN, LOW);
    
    // Attach PWM to Buzzer Pin (channel 0, base freq 2000Hz, 8-bit resolution)
    ledcSetup(BUZZER_CHANNEL, 2000, BUZZER_RESOLUTION);
    ledcAttachPin(PIN_LED_GREEN, BUZZER_CHANNEL);
    ledcWriteTone(BUZZER_CHANNEL, 0); // off
}"""

new_begin = """    pinMode(PIN_LED_GREEN, OUTPUT); // Buzzer
    digitalWrite(PIN_LED_GREEN, LOW);
    
    // Setup PWM channel but don't attach yet until needed
    ledcSetup(BUZZER_CHANNEL, 2000, BUZZER_RESOLUTION);
}"""

fc = fc.replace(old_begin, new_begin)

with open('src/FeedbackManager.cpp', 'w') as f:
    f.write(fc)
print("FeedbackManager PWM hot-plug fixed")
