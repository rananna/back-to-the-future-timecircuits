/**
 * @file DataManager.h
 * @brief Public interface for fetching external data like weather and Data Link content.
 * @details This file declares the functions and data structures responsible for initiating
 * asynchronous data fetching tasks. It handles fetching weather data from an API and
 * content for the "Data Link" display mode from various sources like MQTT or Home Assistant.
 */
#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <ArduinoJson.h>
#include <string>

/**
 * @brief Parameters for the weather data fetching task.
 * @details This structure is used to pass all necessary information to the FreeRTOS task
 * that handles the weather API requests, ensuring the task has all the context it needs
 * to perform geocoding and data retrieval.
 */
struct WeatherTaskParams {
    std::string cityName;   /**< The name of the city for which to fetch weather. Used for geocoding if lat/lon are not provided. */
    bool forceGeocode;      /**< If true, forces a new geocoding lookup even if latitude and longitude are already known. */
    float latitude;         /**< The latitude of the location. */
    float longitude;        /**< The longitude of the location. */
};

/**
 * @brief Initiates the process of fetching data for all enabled "Data Link" points.
 * @details This function iterates through the configured data points and triggers the
 * appropriate fetching mechanism based on the source (e.g., MQTT, Home Assistant).
 */
void fetchDataLink();

/**
 * @brief The core function that performs the weather data fetching and parsing.
 * @param params A pointer to a `WeatherTaskParams` struct containing the location information.
 */
void fetchWeatherData(struct WeatherTaskParams* params);

/**
 * @brief A FreeRTOS task wrapper for fetching weather data.
 * @details This function is designed to be run as a FreeRTOS task. It creates the
 * `WeatherTaskParams` from the current settings and calls `fetchWeatherData`.
 * @param p A void pointer to task parameters (not used).
 */
void fetchWeatherDataTask(void* p);

/**
 * @brief A FreeRTOS task wrapper that forces a weather data fetch, including geocoding.
 * @details Similar to `fetchWeatherDataTask`, but sets the `forceGeocode` flag to true,
 * which is useful when the city name changes.
 * @param p A void pointer to task parameters (not used).
 */
void forceFetchWeatherDataTask(void* p);

/**
 * @brief URL-encodes a string.
 * @details This utility function is necessary for safely including text, like city names,
 * in HTTP GET request URLs.
 * @param msg The raw string to be encoded.
 * @return An Arduino `String` object containing the URL-encoded text.
 */
String urlEncode(const char* msg);

/**
 * @brief Safely retrieves a nested value from a `JsonVariant`.
 * @details This helper function simplifies accessing deeply nested keys in a JSON object
 * (e.g., "hourly.temperature_2m[0]") without causing a crash if an intermediate key is missing.
 * @param root The root `JsonVariant` to search within.
 * @param path A string representing the nested path, with keys separated by dots.
 * @return The `JsonVariant` of the found value, or a null `JsonVariant` if the path is not valid.
 */
JsonVariant getJsonVariant(JsonVariant root, const char* path);

#endif // DATA_MANAGER_H