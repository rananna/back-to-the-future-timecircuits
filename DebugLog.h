#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward declaration for the mutex-protected printing function
// This will be implemented in HardwareControl.cpp
void safe_printf(const char *format, ...);

// Enum for log levels, providing control over verbosity
typedef enum {
    LOG_LEVEL_NONE,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
} LogLevel;

// --- CONFIGURATION ---
// Set the global log level for the project.
// Messages with a level numerically higher than this will be compiled out,
// saving flash space and CPU cycles in production builds.
#define BTTF_CLOCK_LOG_LEVEL LOG_LEVEL_DEBUG

// --- The Core Logging Macro ---
// This macro automatically captures essential context (timestamp, level, task name, file, line)
// and formats it into a clean, readable log message.
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
