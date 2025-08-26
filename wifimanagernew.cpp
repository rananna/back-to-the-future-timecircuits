#include "WifiManagernew.h"
#include "globals.h"
#include "logger.h"
#include <WiFi.h>
#include "types.h"


bool testWifiAndConnect(const char* ssid, const char* password, unsigned long timeout) {
    logger.push(LOG_INFO, "WIFI", "Testing credentials for %s", ssid);
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
        delay(500);
        logger.push(LOG_INFO, "WIFI", ".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        logger.push(LOG_INFO, "WIFI", "Connection successful!");
        return true;
    } else {
        logger.push(LOG_ERROR, "WIFI", "Connection failed.");
        WiFi.disconnect();
        return false;
    }
}