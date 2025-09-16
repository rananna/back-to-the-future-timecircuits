#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <ArduinoJson.h>
#include <string>

struct WeatherTaskParams {
    std::string cityName;
    bool forceGeocode;
};

void fetchDataLink();
void fetchWeatherData(struct WeatherTaskParams* params);
void fetchWeatherDataTask(void* p);
void forceFetchWeatherDataTask(void* p);
void fetchStockDataTask(void* p);
String urlEncode(const char* msg);
JsonVariant getJsonVariant(JsonVariant root, const char* path);

#endif // DATA_MANAGER_H