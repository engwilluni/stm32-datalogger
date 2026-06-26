#include "rtc_time.h"
#include "records.h"
#include "main.h"

extern RTC_HandleTypeDef hrtc;

/*
 * rtc_get — sub-second timestamp from STM32F1 RTC prescaler down-counter.
 *
 * The F1 RTC has no hardware sub-second register. The prescaler divider
 * DIVL counts DOWN from AsynchPrediv (32767 for LSE) to 0, then the
 * second counter increments.
 *
 * ms = (32767 - DIVL) × 1000 / 32768
 *
 * Atomic retry: if DIVL went UP between two readings, a second boundary
 * was crossed and we repeat. Also reads the second counter twice to catch
 * any CNTH/CNTL carry race.
 */
Timestamp rtc_get(void) {
    RTC_TimeTypeDef t1, t2;
    RTC_DateTypeDef d;
    uint32_t div1, div2;

    do {
        /* F1: read Time before Date (Date latches shadow on F4+, harmless on F1) */
        HAL_RTC_GetTime(&hrtc, &t1, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
        div1 = RTC->DIVL;

        HAL_RTC_GetTime(&hrtc, &t2, RTC_FORMAT_BIN);
        div2 = RTC->DIVL;

        /* If div went UP the counter was reloaded (second boundary crossed) */
    } while (div2 > div1 ||
             t1.Seconds != t2.Seconds ||
             t1.Minutes != t2.Minutes);

    Timestamp ts;
    ts.ms  = (uint16_t)((32767U - div2) * 1000U / 32768U);
    ts.sec = rtc_to_epoch(d.Year, d.Month, d.Date,
                           t2.Hours, t2.Minutes, t2.Seconds);
    return ts;
}

/* ----------------------------------------------------------------
 * rtc_set — set RTC calendar from y2k epoch seconds
 * ---------------------------------------------------------------- */
void rtc_set(uint32_t epoch) {
    DateTime dt;
    epoch_to_dt(epoch, &dt);

    RTC_TimeTypeDef t = {0};
    t.Hours   = (uint8_t)dt.hour;
    t.Minutes = (uint8_t)dt.min;
    t.Seconds = (uint8_t)dt.sec;

    RTC_DateTypeDef d = {0};
    d.Year  = (uint8_t)(dt.year - 2000);
    d.Month = dt.month;
    d.Date  = dt.day;
    d.WeekDay = RTC_WEEKDAY_MONDAY;

    HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
}
