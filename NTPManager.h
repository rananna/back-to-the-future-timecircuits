#ifndef NTPMANAGER_H
#define NTPMANAGER_H

#include <time.h>

class NTPManager {
public:
    NTPManager();
    void init();
    void loop();
    bool isTimeSynchronized();

private:
    bool timeSynchronized;
    unsigned long lastNtpUpdate;
    int currentNtpServerIndex;

    void syncTime();
};

#endif // NTPMANAGER_H
