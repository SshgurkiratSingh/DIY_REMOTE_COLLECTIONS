#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Preferences.h>
#include "Config.h"

class StorageManager
{
public:
    void begin();
    void loadConfig(SystemConfig &config);
    void saveConfig(const SystemConfig &config);
    void factoryReset();

    // Target management
    void addTarget(const char *name, const uint8_t *mac, uint8_t verifyKey);
    int getTargetCount();
    TargetNode getTarget(int index);

    // UI Persistence
    int getLastTargetIndex();
    void setLastTargetIndex(int index);

private:
    Preferences preferences;
};

#endif
