/**
 * @file DataManager.h
 * @brief Handles fetching and parsing of data from external network sources.
 * @details This module contains the logic for making HTTP requests to web APIs
 * and the Open-Meteo weather service. All network operations are performed in
 * dedicated FreeRTOS tasks to avoid blocking the main application loop.
 */

#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <ArduinoJson.h>
#include <string>

/**
 * @struct WeatherTaskParams
 * @brief Parameters for a weather data fetch operation.
 * @details This struct is used to pass arguments to the FreeRTOS task that
 * fetches weather data, ensuring thread-safe operation.
 */
struct WeatherTaskParams {
    std::string cityName; ///< The name of the city for which to fetch weather.
    bool forceGeocode;    ///< If true, forces a new geocoding lookup even if the city name hasn't changed.
};

// --- Function Declarations for data and networking ---

/**
 * @brief Manages the periodic fetching of data for all configured Data Link marquee slots.
 * @details This function is non-blocking. It checks if a refresh is needed and, if so,
 * spawns a separate FreeRTOS task for each API-based data point to fetch its data.
 */
void fetchDataLink();

/**
 * @brief Fetches weather data from the Open-Meteo API. Runs in a dedicated FreeRTOS task.
 * @details This function performs two main steps:
 * 1. (If necessary) Geocodes the city name to get latitude and longitude.
 * 2. Fetches the detailed weather forecast for those coordinates.
 * The results are stored in the global `currentWeatherData` struct.
 * @param params A pointer to a WeatherTaskParams struct containing the city name and geocoding options.
 */
void fetchWeatherData(struct WeatherTaskParams* params);

/**
 * @brief A wrapper function to create a FreeRTOS task for a standard weather data fetch.
 * @param p A void pointer to the task parameters (not used directly, but required by FreeRTOS task signature).
 */
void fetchWeatherDataTask(void* p);

/**
 * @brief A wrapper function to create a FreeRTOS task that forces a weather data fetch and geocoding lookup.
 * @param p A void pointer to a WeatherTaskParams struct.
 */
void forceFetchWeatherDataTask(void* p);

/**
 * @brief URL-encodes a string.
 * @param msg The C-string to be URL-encoded.
 * @return An Arduino String object containing the URL-encoded text.
 */
String urlEncode(const char* msg);

/**
 * @brief Safely retrieves a nested value from a JSON object using a dot-and-bracket path.
 * @param root The root ArduinoJson::JsonVariant to search within.
 * @param path A C-string representing the path (e.g., "results[0].name").
 * @return The found JsonVariant, or a null JsonVariant if the path is not found.
 */
JsonVariant getJsonVariant(JsonVariant root, const char* path);

#endif // DATA_MANAGER_H