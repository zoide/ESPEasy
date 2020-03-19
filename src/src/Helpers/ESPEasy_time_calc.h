#ifndef HELPERS_ESPEASY_TIME_CALC_H
#define HELPERS_ESPEASY_TIME_CALC_H

#include <Arduino.h>

/********************************************************************************************\
   Simple time computations.
 \*********************************************************************************************/

// Return the time difference as a signed value, taking into account the timers may overflow.
// Returned timediff is between -24.9 days and +24.9 days.
// Returned value is positive when "next" is after "prev"
long ICACHE_RAM_ATTR timeDiff(const unsigned long prev, const unsigned long next);

// Compute the number of milliSeconds passed since timestamp given.
// N.B. value can be negative if the timestamp has not yet been reached.
long timePassedSince(unsigned long timestamp);

long usecPassedSince(unsigned long timestamp);

// Check if a certain timeout has been reached.
bool timeOutReached(unsigned long timer);

bool usecTimeOutReached(unsigned long timer);





/********************************************************************************************\
   Unix Time computations
 \*********************************************************************************************/
bool isLeapYear(int year);

uint32_t makeTime(const struct tm& tm);

/********************************************************************************************\
   Time computations for rules.
 \*********************************************************************************************/

// format 0000WWWWAAAABBBBCCCCDDDD
// WWWW=weekday, AAAA=hours tens digit, BBBB=hours, CCCC=minutes tens digit DDDD=minutes

// Convert a 32 bit integer into a string like "Sun,12:30"
String timeLong2String(unsigned long lngTime);

// Convert a string like "Sun,12:30" into a 32 bit integer
unsigned long string2TimeLong(const String& str);


/********************************************************************************************\
   Match clock event
 \*********************************************************************************************/
bool matchClockEvent(unsigned long clockEvent, unsigned long clockSet);


#endif // HELPERS_ESPEASY_TIME_CALC_H