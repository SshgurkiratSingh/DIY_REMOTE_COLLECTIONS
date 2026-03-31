#include "PortalManager.h"

// HTML string
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin:0px auto; padding-top: 30px;}
    input[type=text] { width: 80%; padding: 12px 20px; margin: 8px 0; display: inline-block; border: 1px solid #ccc; box-sizing: border-box; }
    input[type=submit] { background-color: #4CAF50; color: white; padding: 14px 20px; margin: 8px 0; border: none; cursor: pointer; width: 80%; }
  </style>
</head><body>
  <h2>Add New Target</h2>
  <form action="/save" method="post">
    <label>Target Name (Max 15 char)</label><br>
    <input type="text" name="name" maxlength="15" required><br>
    <label>MAC Address (e.g. 24:6F:28:XX:YY:ZZ)</label><br>
    <input type="text" name="mac" pattern="^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$" placeholder="XX:XX:XX:XX:XX:XX" required><br>
    <input type="submit" value="Save Target">
  </form>
</body></html>)rawliteral";

void PortalManager::startPortal(StorageManager* storageMgr, DisplayManager* displayMgr) {
    this->stMgr = storageMgr;
    this->dispMgr = displayMgr;
    portalRunning = true;
    
    // Shut down ESP-NOW via WiFi restart
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP-NOW-REMOTE", "12345678");

    // Standard Captive portal DNS to trap all calls
    dnsServer.start(53, "*", WiFi.softAPIP());

    server.on("/", HTTP_GET, [this]() {
        server.send(200, "text/html", INDEX_HTML);
    });

    server.on("/save", HTTP_POST, [this]() {
        handleSave();
    });

    server.onNotFound([this]() {
        server.send(200, "text/html", INDEX_HTML); // Redirect everything to captive HTML
    });

    server.begin();
    
    // Show on display
    // Update dispMgr to show AP info explicitly if it had a method... For now we bypass it later.
}

void PortalManager::handleClient() {
    if (portalRunning) {
        dnsServer.processNextRequest();
        server.handleClient();
    }
}

void PortalManager::handleSave() {
    if (server.hasArg("name") && server.hasArg("mac")) {
        String name = server.arg("name");
        String macStr = server.arg("mac");
        
        // Parse MAC
        uint8_t mac[6];
        int values[6];
        if (6 == sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x", 
                        &values[0], &values[1], &values[2], 
                        &values[3], &values[4], &values[5])) {
            for(int i = 0; i < 6; ++i) mac[i] = (uint8_t)values[i];
            
            stMgr->addTarget(name.c_str(), mac);
            
            String response = "<h2>Target Saved!</h2><p>Rebooting...</p>";
            server.send(200, "text/html", response);
            delay(1000);
            ESP.restart(); // Restart out of AP Mode
        } else {
            server.send(400, "text/plain", "Invalid MAC formatting.");
        }
    } else {
        server.send(400, "text/plain", "Missing args");
    }
}

