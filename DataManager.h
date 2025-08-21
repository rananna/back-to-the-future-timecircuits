#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <ArduinoJson.h>
#include <string>

// Struct for weather task parameters - NOW DEFINED HERE
struct WeatherTaskParams {
    std::string cityName;
    bool forceGeocode;
};

// Function Declarations for data and networking
void fetchDataLink();
void fetchWeatherData(struct WeatherTaskParams* params);
void fetchWeatherDataTask(void* p);
void forceFetchWeatherDataTask(void* p);
String urlEncode(const char* msg);
JsonVariant getJsonVariant(JsonVariant root, const char* path);

#endif // DATA_MANAGER_H