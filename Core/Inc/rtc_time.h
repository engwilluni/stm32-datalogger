#ifndef RTC_TIME_H
#define RTC_TIME_H

#include <stdint.h>

typedef struct {
    uint32_t sec;  /* seconds since 2000-01-01 00:00:00 (y2k epoch) */
    uint16_t ms;   /* milliseconds within second */
} Timestamp;

/* Atomic RTC read with sub-second precision from RTC prescaler down-counter.
   Retries if a second boundary was crossed during the read. */
Timestamp rtc_get(void);

/* Set RTC calendar from y2k epoch seconds */
void rtc_set(uint32_t epoch);

#endif /* RTC_TIME_H */
