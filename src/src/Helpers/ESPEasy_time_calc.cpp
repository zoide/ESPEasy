#include "ESPEasy_time_calc.h"

#include <Arduino.h>
#include <limits.h>

#include "../Globals/ESPEasy_time.h"

// FIXME TD-er: Needed for GetArgv
#include "../../ESPEasy_fdwdecl.h"


#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)
#define LEAP_YEAR(Y) (((1970 + Y) > 0) && !((1970 + Y) % 4) && (((1970 + Y) % 100) || !((1970 + Y) % 400)))

long ICACHE_RAM_ATTR timeDiff(const unsigned long prev, const unsigned long next)
{
  long signed_diff = 0;

  // To cast a value to a signed long, the difference may not exceed half the ULONG_MAX
  const unsigned long half_max_unsigned_long = 2147483647u; // = 2^31 -1

  if (next >= prev) {
    const unsigned long diff = next - prev;

    if (diff <= half_max_unsigned_long) {
      // Normal situation, just return the difference.
      // Difference is a positive value.
      signed_diff = static_cast<long>(diff);
    } else {
      // prev has overflow, return a negative difference value
      signed_diff = static_cast<long>((ULONG_MAX - next) + prev + 1u);
      signed_diff = -1 * signed_diff;
    }
  } else {
    // next < prev
    const unsigned long diff = prev - next;

    if (diff <= half_max_unsigned_long) {
      // Normal situation, return a negative difference value
      signed_diff = static_cast<long>(diff);
      signed_diff = -1 * signed_diff;
    } else {
      // next has overflow, return a positive difference value
      signed_diff = static_cast<long>((ULONG_MAX - prev) + next + 1u);
    }
  }
  return signed_diff;
}


long timePassedSince(unsigned long timestamp) {
  return timeDiff(timestamp, millis());
}

long usecPassedSince(unsigned long timestamp) {
  return timeDiff(timestamp, micros());
}

// Check if a certain timeout has been reached.
bool timeOutReached(unsigned long timer) {
  const long passed = timePassedSince(timer);

  return passed >= 0;
}

bool usecTimeOutReached(unsigned long timer) {
  const long passed = usecPassedSince(timer);

  return passed >= 0;
}


bool isLeapYear(int year) {
  return LEAP_YEAR(year);
}

/********************************************************************************************\
   Unix Time computations
 \*********************************************************************************************/

uint32_t makeTime(const struct tm& tm) {
  // assemble time elements into uint32_t
  // note year argument is offset from 1970 (see macros in time.h to convert to other formats)
  // previous version used full four digit year (or digits since 2000),i.e. 2009 was 2009 or 9
  const uint8_t monthDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  int i;
  uint32_t seconds;

  // seconds from 1970 till 1 jan 00:00:00 of the given year
  seconds = tm.tm_year * (SECS_PER_DAY * 365);

  for (i = 0; i < tm.tm_year; i++) {
    if (isLeapYear(i)) {
      seconds +=  SECS_PER_DAY; // add extra days for leap years
    }
  }

  // add days for this year, months start from 1
  for (i = 1; i < tm.tm_mon; i++) {
    if ((i == 2) && isLeapYear(tm.tm_year)) {
      seconds += SECS_PER_DAY * 29;
    } else {
      seconds += SECS_PER_DAY * monthDays[i - 1]; // monthDay array starts from 0
    }
  }
  seconds += (tm.tm_mday - 1) * SECS_PER_DAY;
  seconds += tm.tm_hour * SECS_PER_HOUR;
  seconds += tm.tm_min * SECS_PER_MIN;
  seconds += tm.tm_sec;
  return (uint32_t)seconds;
}


/********************************************************************************************\
   Time computations for rules.
 \*********************************************************************************************/

String timeLong2String(unsigned long lngTime)
{
  unsigned long x = 0;
  String time     = "";

  x = (lngTime >> 16) & 0xf;

  if (x == 0x0f) {
    x = 0;
  }
  String weekDays = F("AllSunMonTueWedThuFriSatWrkWkd");
  time  = weekDays.substring(x * 3, x * 3 + 3);
  time += ",";

  x = (lngTime >> 12) & 0xf;

  if (x == 0xf) {
    time += "*";
  }
  else if (x == 0xe) {
    time += '-';
  }
  else {
    time += x;
  }

  x = (lngTime >> 8) & 0xf;

  if (x == 0xf) {
    time += "*";
  }
  else if (x == 0xe) {
    time += '-';
  }
  else {
    time += x;
  }

  time += ":";

  x = (lngTime >> 4) & 0xf;

  if (x == 0xf) {
    time += "*";
  }
  else if (x == 0xe) {
    time += '-';
  }
  else {
    time += x;
  }

  x = (lngTime) & 0xf;

  if (x == 0xf) {
    time += "*";
  }
  else if (x == 0xe) {
    time += '-';
  }
  else {
    time += x;
  }

  return time;
}


unsigned long string2TimeLong(const String& str)
{
  // format 0000WWWWAAAABBBBCCCCDDDD
  // WWWW=weekday, AAAA=hours tens digit, BBBB=hours, CCCC=minutes tens digit DDDD=minutes

  char command[20];
  int  w, x, y;
  unsigned long a;
  {
    // Within a scope so the tmpString is only used for copy.
    String tmpString(str);
    tmpString.toLowerCase();
    tmpString.toCharArray(command, 20);
  }
  unsigned long lngTime = 0;
  String TmpStr1;

  if (GetArgv(command, TmpStr1, 1))
  {
    String day      = TmpStr1;
    String weekDays = F("allsunmontuewedthufrisatwrkwkd");
    y = weekDays.indexOf(TmpStr1) / 3;

    if (y == 0) {
      y = 0xf; // wildcard is 0xf
    }
    lngTime |= (unsigned long)y << 16;
  }

  if (GetArgv(command, TmpStr1, 2))
  {
    y = 0;

    for (x = TmpStr1.length() - 1; x >= 0; x--)
    {
      w = TmpStr1[x];

      if (((w >= '0') && (w <= '9')) || (w == '*'))
      {
        a        = 0xffffffff  ^ (0xfUL << y); // create mask to clean nibble position y
        lngTime &= a;                          // maak nibble leeg

        if (w == '*') {
          lngTime |= (0xFUL << y);             // fill nibble with wildcard value
        }
        else {
          lngTime |= (w - '0') << y;           // fill nibble with token
        }
        y += 4;
      }
      else
      if (w == ':') {}
      else
      {
        break;
      }
    }
  }
  #undef TmpStr1Length
  return lngTime;
}



/********************************************************************************************\
   Match clock event
 \*********************************************************************************************/
bool matchClockEvent(unsigned long clockEvent, unsigned long clockSet)
{
  unsigned long Mask;

  for (byte y = 0; y < 8; y++)
  {
    if (((clockSet >> (y * 4)) & 0xf) == 0xf)         // if nibble y has the wildcard value 0xf
    {
      Mask        = 0xffffffff  ^ (0xFUL << (y * 4)); // Mask to wipe nibble position y.
      clockEvent &= Mask;                             // clear nibble
      clockEvent |= (0xFUL << (y * 4));               // fill with wildcard value 0xf
    }
  }

  if (((clockSet >> (16)) & 0xf) == 0x8) {         // if weekday nibble has the wildcard value 0x8 (workdays)
    if (node_time.weekday() >= 2 and node_time.weekday() <= 6)         // and we have a working day today...
    {
      Mask        = 0xffffffff  ^ (0xFUL << (16)); // Mask to wipe nibble position.
      clockEvent &= Mask;                          // clear nibble
      clockEvent |= (0x8UL << (16));               // fill with wildcard value 0x8
    }
  }

  if (((clockSet >> (16)) & 0xf) == 0x9) {         // if weekday nibble has the wildcard value 0x9 (weekends)
    if (node_time.weekday() == 1 or node_time.weekday() == 7)          // and we have a weekend day today...
    {
      Mask        = 0xffffffff  ^ (0xFUL << (16)); // Mask to wipe nibble position.
      clockEvent &= Mask;                          // clear nibble
      clockEvent |= (0x9UL << (16));               // fill with wildcard value 0x9
    }
  }

  if (clockEvent == clockSet) {
    return true;
  }
  return false;
}
