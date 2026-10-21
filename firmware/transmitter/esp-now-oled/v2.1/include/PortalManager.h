#ifndef PORTAL_MANAGER_H
#define PORTAL_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "Config.h"
#include "StorageManager.h"
#include "DisplayManager.h"

class PortalManager {
public:
    void startPortal(StorageManager* storageMgr, DisplayManager* displayMgr);
    void handleClient();
    bool isRunning() { return portalRunning; }
private:
    bool portalRunning = false;
    DNSServer dnsServer;
    WebServer server{80};
    StorageManager* stMgr;
    DisplayManager* dispMgr;
    
    void handleRoot();
    void handleSave();
};

#endif
