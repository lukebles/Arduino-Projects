#ifndef TIME_MANAGEMENT_H
#define TIME_MANAGEMENT_H

#include <TimeLib.h>

void setSystemTime(int year, int month, int day, int hour, int minute, int second) {
    setTime(hour, minute, second, day, month, year);
}

#endif // TIME_MANAGEMENT_H
