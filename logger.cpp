#include "Logger.h"

Logger::Logger() {
}

void Logger::begin() {
}

void Logger::setLevel(LogLevel level) {
  this->currentLevel = level;
}

void Logger::enableLogging(bool enable) {
  this->loggingEnabled = enable;
}

bool Logger::isLoggingEnabled() {
  return this->loggingEnabled;
}

void Logger::printf(LogLevel level, const char *format, ...) {
  if (!loggingEnabled || level > currentLevel) {
    return;
  }

  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  char timedBuf[300];
  snprintf(timedBuf, sizeof(timedBuf), "[%lu] %s", millis(), buf);

  Serial.print(timedBuf);
  WebSerial.print(timedBuf);
}

void Logger::loop() {
}