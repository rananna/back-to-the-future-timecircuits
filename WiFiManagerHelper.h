#ifndef WIFIMANAGERHELPER_H
#define WIFIMANAGERHELPER_H

#include <WiFi.h>
#include <WiFiManager.h>

class WiFiManagerHelper {
public:
    enum WifiState {
        WIFI_STATE_CONNECTING,
        WIFI_STATE_START_PORTAL,
        WIFI_STATE_PORTAL_RUNNING,
        WIFI_STATE_CONNECTED
    };

    WiFiManagerHelper();
    void init();
    void loop();
    bool isConnected();

private:
    WifiState wifiState;
    unsigned long wifiConnectStartTime;
    TaskHandle_t wifiManagerTaskHandle;
    WiFiManager wifiManager;

    void startPortalTask();
    static void wifiManagerTask(void *pvParameters);
};

#endif // WIFIMANAGERHELPER_H
