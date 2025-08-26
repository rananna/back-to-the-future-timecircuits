#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <vector>
#include <string>
#include <freertos/semphr.h>
#include "types.h" // Includes LogLevel

#define MAX_LOG_ENTRIES 50

// Define a function pointer type for the broadcast callback
typedef void (*LogBroadcastCallback)(const char* message);

struct LogEntry {
    unsigned long timestamp;
    LogLevel level;
    char source[12];
    char message[128];
};

class Logger {
public:
    Logger();
    ~Logger();

    void setBroadcastCallback(LogBroadcastCallback callback);
    void push(LogLevel level, const char* source, const char* format, ...);
    void pushMessage(LogLevel level, const char* source, const char* message); // Renamed function
    void clear();
    int getLogCount();
    LogEntry getEntry(int index);

private:
    LogEntry* logBuffer = nullptr;
    int head = 0;
    int count = 0;
    SemaphoreHandle_t xLogMutex;
    LogBroadcastCallback broadcastCallback = nullptr;
    int logCount = 0;
};

#endif // LOGGER_H