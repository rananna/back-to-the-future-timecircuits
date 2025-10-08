/**
 * @file api_templates.h
 * @brief Declares the PROGMEM variable holding API testing templates.
 * @details This header provides a forward declaration for the `apiTemplates` variable,
 * which is a large string literal stored in the ESP32's program memory (flash)
 * instead of RAM. This is crucial for saving precious RAM on memory-constrained
 * devices. The actual definition and content of the string are located in a .cpp file.
 */
#ifndef API_TEMPLATES_H
#define API_TEMPLATES_H

#include <pgmspace.h>

/**
 * @brief An `extern` declaration for a large JSON string stored in PROGMEM (flash memory).
 * @details This variable holds a JSON string containing templates for testing various APIs
 * from the web UI. Declaring it as `extern` allows multiple files to access it, while the
 * actual data is defined and allocated only once in a corresponding .cpp file. The `PROGMEM`
 * attribute is key to conserving RAM.
 */
extern const char apiTemplates[] PROGMEM;

#endif // API_TEMPLATES_H