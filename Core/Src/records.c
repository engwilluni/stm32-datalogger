#include "records.h"
#include "cmsis_os.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* ----------------------------------------------------------------
 * CRC-8/MAXIM (poly 0x31, init 0x00) — used per record
 * ---------------------------------------------------------------- */
uint8_t rec_crc8(const uint8_t *buf, size_t len) {
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *buf++;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x80) ? ((uint8_t)(crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* ----------------------------------------------------------------
 * CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) — used for header
 * ---------------------------------------------------------------- */
uint16_t rec_crc16(const uint8_t *buf, size_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= (uint16_t)(*buf++) << 8;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    return crc;
}

/* ----------------------------------------------------------------
 * CRC-32/IEEE (poly 0x04C11DB7, init 0xFFFFFFFF, final XOR 0xFFFFFFFF)
 * ---------------------------------------------------------------- */
uint32_t rec_crc32(const uint8_t *buf, size_t len) {
    uint32_t crc = 0xFFFFFFFFU;
    while (len--) {
        crc ^= (uint32_t)(*buf++) << 24;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x80000000U) ? ((crc << 1) ^ 0x04C11DB7U) : (crc << 1);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

/* ----------------------------------------------------------------
 * Sequence counter — atomic via critical section
 * ---------------------------------------------------------------- */
static uint8_t s_seq = 0;

uint8_t rec_next_seq(void) {
    uint8_t s;
    taskENTER_CRITICAL();
    s = s_seq++;
    taskEXIT_CRITICAL();
    return s;
}

/* ----------------------------------------------------------------
 * rec_seal — fill the crc8 field of a record
 * ---------------------------------------------------------------- */
void rec_seal(LogRecord *r) {
    r->crc8 = rec_crc8((const uint8_t *)r, offsetof(LogRecord, crc8));
}

/* ----------------------------------------------------------------
 * rec_fill_header — populate and seal a LogHeader
 * ---------------------------------------------------------------- */
void rec_fill_header(LogHeader *h, uint32_t epoch_base, uint16_t session, uint8_t is_csv) {
    memset(h, 0, sizeof(*h));
    h->magic[0] = 'S'; h->magic[1] = 'D'; h->magic[2] = 'L'; h->magic[3] = 'G';
    h->version    = 1;
    h->rec_size   = sizeof(LogRecord);
    h->epoch_base = epoch_base;
    h->session_id = session;
    h->flags      = is_csv ? 1u : 0u;
    h->fw_ver     = 0x0001;
    h->crc16 = rec_crc16((const uint8_t *)h, offsetof(LogHeader, crc16));
}

/* ----------------------------------------------------------------
 * Epoch helpers (y2k epoch = seconds since 2000-01-01 00:00:00)
 * ---------------------------------------------------------------- */
static int is_leap(uint16_t y) {
    /* valid for 2000-2099: all multiples of 4 are leap years */
    return (y % 4 == 0);
}
static const uint8_t s_mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

uint32_t rtc_to_epoch(uint8_t year2k, uint8_t month, uint8_t day,
                       uint8_t hour,   uint8_t min,   uint8_t sec) {
    uint32_t days = 0;
    for (int y = 0; y < (int)year2k; y++) {
        days += is_leap(2000 + y) ? 366u : 365u;
    }
    uint16_t yr = 2000 + year2k;
    for (int m = 1; m < (int)month; m++) {
        days += s_mdays[m-1] + (m == 2 && is_leap(yr) ? 1u : 0u);
    }
    days += (uint32_t)(day - 1);
    return days * 86400UL + (uint32_t)hour * 3600UL + (uint32_t)min * 60UL + sec;
}

void epoch_to_dt(uint32_t epoch, DateTime *dt) {
    dt->sec   = (uint8_t)(epoch % 60); epoch /= 60;
    dt->min   = (uint8_t)(epoch % 60); epoch /= 60;
    dt->hour  = (uint8_t)(epoch % 24); epoch /= 24;
    dt->year  = 2000;
    for (;;) {
        uint32_t yd = is_leap(dt->year) ? 366u : 365u;
        if (epoch < yd) break;
        epoch -= yd;
        dt->year++;
    }
    for (dt->month = 1; dt->month <= 12; dt->month++) {
        uint8_t md = s_mdays[dt->month - 1] + (dt->month == 2 && is_leap(dt->year) ? 1 : 0);
        if (epoch < md) break;
        epoch -= md;
    }
    dt->day = (uint8_t)(epoch + 1);
}

/* ----------------------------------------------------------------
 * rec_to_csv — format one record as an ISO-8601 CSV line
 * Format: YYYY-MM-DDThh:mm:ss.mmm,TYPE,chan,value\n
 * Returns number of chars written (without NUL).
 * ---------------------------------------------------------------- */
int rec_to_csv(const LogRecord *r, char *buf, size_t buflen) {
    DateTime dt;
    epoch_to_dt(r->t_sec, &dt);

    const char *type_str;
    switch (r->type) {
    case REC_TYPE_ADC:    type_str = "ADC";    break;
    case REC_TYPE_GPIO:   type_str = "GPIO";   break;
    case REC_TYPE_MARKER: type_str = "MARKER"; break;
    default:              type_str = "LOSS";   break;
    }

    return snprintf(buf, buflen,
        "%04u-%02u-%02uT%02u:%02u:%02u.%03u,%s,%u,%u\n",
        dt.year, (unsigned)dt.month, (unsigned)dt.day,
        (unsigned)dt.hour, (unsigned)dt.min, (unsigned)dt.sec,
        (unsigned)r->t_ms,
        type_str,
        (unsigned)r->chan,
        (unsigned)r->value);
}
