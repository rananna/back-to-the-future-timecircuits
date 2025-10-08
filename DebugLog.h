/**
 * @file DebugLog.h
 * @brief A context-aware, thread-safe, and compile-time-configurable logging framework.
 * @details This file provides a powerful logging macro (`Log_printf`) that enhances standard
 * `printf`-style logging with automatic inclusion of timestamp, log level, FreeRTOS task name,
 * filename, and line number. It is thread-safe through its use of the `safe_printf` function,
 * which employs a mutex. The logging verbosity can be set at compile time, allowing debug
 * messages to be completely removed from production builds to save flash space and CPU cycles.
 */
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward declaration for the mutex-protected printing function, which is defined in HardwareControl.cpp.
void safe_printf(const char *format, ...);

/**
 * @brief Defines the available levels for log messages.
 * @details This allows for granular control over logging verbosity.
 */
typedef enum {
    LOG_LEVEL_NONE,     /**< No logs will be output. */
    LOG_LEVEL_ERROR,    /**< Only critical errors. */
    LOG_LEVEL_WARN,     /**< Warnings and errors. */
    LOG_LEVEL_INFO,     /**< Informational messages, warnings, and errors. */
    LOG_LEVEL_DEBUG     /**< All messages, including detailed debug info. */
} LogLevel;


/**
 * @brief Sets the global log level for the entire project at compile time.
 * @details Messages with a log level numerically higher than this value will be
 * completely compiled out by the preprocessor, resulting in zero performance overhead
 * and no flash space usage in production builds where logging is not needed.
 * For example, setting this to `LOG_LEVEL_INFO` will include ERROR, WARN, and INFO
 * messages, but exclude DEBUG messages.
 */
#define BTTF_CLOCK_LOG_LEVEL LOG_LEVEL_DEBUG

/**
 * @brief The core logging macro that provides context-rich, thread-safe output.
 * @details This macro is the primary interface for logging throughout the firmware.
 * It automatically captures essential context (timestamp, log level, task name, file, line)
 * and formats it into a clean, readable log message. It uses a `do-while(0)` loop to ensure
 * it behaves like a single statement.
 *
 * Example usage:
 * `Log_printf(LOG_LEVEL_INFO, "WiFi connected with IP: %s", ipAddress);`
 *
 * Example output:
 * `[  12345] [INFO ] [wifi_task       ] src/main.cpp:150: WiFi connected with IP: 192.168.1.100`
 *
 * @param level The `LogLevel` of the message (e.g., `LOG_LEVEL_ERROR`).
 * @param format The `printf`-style format string.
 * @param ... The variable arguments corresponding to the format string.
 */
#define Log_printf(level, format, ...) \
    do { \
        if (level <= BTTF_CLOCK_LOG_LEVEL) { \
            const char* task_name = pcTaskGetTaskName(NULL); \
            const char* level_str = (level == LOG_LEVEL_ERROR) ? "ERROR" : \
                                    (level == LOG_LEVEL_WARN)  ? "WARN"  : \
                                    (level == LOG_LEVEL_INFO)  ? "INFO"  : "DEBUG"; \
            safe_printf("[%7lu] [%-5s] [%-16s] %s:%d: " format "\r\n", \
                millis(), \
                level_str, \
                task_name ? task_name : "no_task", \
                __FILE__, \
                __LINE__, \
                ##__VA_ARGS__); \
        } \
    } while(0)

#endif // DEBUG_LOG_H
