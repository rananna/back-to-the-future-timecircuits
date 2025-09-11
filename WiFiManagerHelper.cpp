#include "WiFiManagerHelper.h"
#include <esp_log.h>

#define WIFI_CONNECT_TIMEOUT 15000

WiFiManagerHelper::WiFiManagerHelper() :
    wifiState(WIFI_STATE_CONNECTING),
    wifiConnectStartTime(0),
    wifiManagerTaskHandle(NULL) {
}

void WiFiManagerHelper::init() {
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    wifiConnectStartTime = millis();
    ESP_LOGI("WIFI", "Non-blocking WiFi connection initiated...");
}

bool WiFiManagerHelper::isConnected() {
    return wifiState == WIFI_STATE_CONNECTED;
}

void WiFiManagerHelper::loop() {
    switch (wifiState) {
        case WIFI_STATE_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                ESP_LOGI("WIFI", "WiFi connected. IP: %s", WiFi.localIP().toString().c_str());
                wifiState = WIFI_STATE_CONNECTED;
                // Optionally, you can have a callback function here to notify the main app
            } else if (millis() - wifiConnectStartTime > WIFI_CONNECT_TIMEOUT) {
                ESP_LOGI("WIFI", "WiFi connection timed out. Starting portal.");
                wifiState = WIFI_STATE_START_PORTAL;
            }
            break;

        case WIFI_STATE_START_PORTAL:
            startPortalTask();
            wifiState = WIFI_STATE_PORTAL_RUNNING;
            break;

        case WIFI_STATE_PORTAL_RUNNING:
            if (WiFi.status() == WL_CONNECTED) {
                ESP_LOGI("WIFI", "WiFi connected via portal. Rebooting...");
                if(wifiManagerTaskHandle != NULL) {
                    vTaskDelete(wifiManagerTaskHandle);
                    wifiManagerTaskHandle = NULL;
                }
                // Delay to ensure message is sent/logged before reboot
                delay(2000);
                ESP.restart();
            }
            break;

        case WIFI_STATE_CONNECTED:
            // Do nothing, already connected.
            break;
    }
}

void WiFiManagerHelper::startPortalTask() {
    xTaskCreate(
        this->wifiManagerTask,
        "WiFiManagerPortal",
        4096,
        &wifiManager, // Pass the instance of WiFiManager
        1,
        &wifiManagerTaskHandle
    );
}

void WiFiManagerHelper::wifiManagerTask(void *pvParameters) {
    WiFiManager* wm = (WiFiManager*)pvParameters;
    wm->autoConnect("BTTF-Clock-Setup");
    // The task will be deleted by the main loop logic when connection is established.
    vTaskDelete(NULL);
}
