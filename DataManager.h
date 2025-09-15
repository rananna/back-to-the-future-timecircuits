#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <ArduinoJson.h>
#include <string>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// Use a queue to send parameters to the persistent weather task
struct WeatherTaskParams {
    std::string cityName;
    bool forceGeocode;
};

extern QueueHandle_t xWeatherQueue;
extern SemaphoreHandle_t xWeatherSemaphore;

void createWeatherTask();
void triggerWeatherFetch(bool forceGeocode = false);
void fetchWeatherDataTask(void* p); // Persistent task
void forceFetchWeatherDataTask(void* p); // One-off task

// Unchanged functions
void fetchDataLink();
void fetchStockDataTask(void* p);
String urlEncode(const char* msg);
JsonVariant getJsonVariant(JsonVariant root, const char* path);

#endif // DATA_MANAGER_H