#include "logger.h"
#include <Arduino.h>
#include <string.h>

Logger::Logger() {
    logBuffer = new LogEntry[MAX_LOG_ENTRIES];
    xLogMutex = xSemaphoreCreateMutex();
}

Logger::~Logger() {
    delete[] logBuffer;
}

void Logger::setBroadcastCallback(LogBroadcastCallback callback) {
    broadcastCallback = callback;
}

void Logger::push(LogLevel level, const char* source, const char* format, ...) {
    if(xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
        // Use a temporary buffer to format the message
        char formattedMessage[128];
        va_list args;
        va_start(args, format);
        vsnprintf(formattedMessage, sizeof(formattedMessage), format, args);
        va_end(args);

        // Call the other overload with the formatted message
        pushMessage(level, source, (const char*)formattedMessage);

        xSemaphoreGive(xLogMutex);
    }
}

void Logger::pushMessage(LogLevel level, const char* source, const char* message) {
    if(xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
        LogEntry* entry = &logBuffer[head];
        entry->timestamp = millis();
        entry->level = level;
        strncpy(entry->source, source, sizeof(entry->source) - 1);
        entry->source[sizeof(entry->source) - 1] = '\0';
        strncpy(entry->message, message, sizeof(entry->message) - 1);
        entry->message[sizeof(entry->message) - 1] = '\0';
        head = (head + 1) % MAX_LOG_ENTRIES;
        if (count < MAX_LOG_ENTRIES) {
            count++;
        }
        
        if (broadcastCallback) {
            broadcastCallback(entry->message);
        }
        
        xSemaphoreGive(xLogMutex);
    }
}

void Logger::clear() {
    if(xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
        head = 0;
        count = 0;
        xSemaphoreGive(xLogMutex);
    }
}

int Logger::getLogCount() {
    int currentCount;
    if(xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
        currentCount = count;
        xSemaphoreGive(xLogMutex);
    }
    return currentCount;
}

LogEntry Logger::getEntry(int index) {
    LogEntry entry;
    if(xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
        int realIndex = (head + index - count + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
        entry = logBuffer[realIndex];
        xSemaphoreGive(xLogMutex);
    }
    return entry;
}