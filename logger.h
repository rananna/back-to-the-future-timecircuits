#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <WebSerial.h>

enum LogLevel {
  LOG_LEVEL_NONE,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_WARN,
  LOG_LEVEL_INFO,
  LOG_LEVEL_DEBUG
};

class Logger {
public:
  static Logger& getInstance() {
    static Logger instance;
    return instance;
  }

  void begin();
  void setLevel(LogLevel level);
  void printf(LogLevel level, const char *format, ...);
  void loop();

  void enableLogging(bool enable);
  bool isLoggingEnabled();

private:
  Logger();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  LogLevel currentLevel = LOG_LEVEL_INFO;
  bool loggingEnabled = true;
};

#define Log Logger::getInstance()

#endif // LOGGER_H