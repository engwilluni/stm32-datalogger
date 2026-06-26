#ifndef RECORDS_H
#define RECORDS_H

#include <stdint.h>
#include <stddef.h>

/* ---- Record types ---- */
#define REC_TYPE_ADC    0u
#define REC_TYPE_GPIO   1u
#define REC_TYPE_MARKER 2u
#define REC_TYPE_LOSS   3u

/* ---- 12-byte log record ---- */
typedef struct __attribute__((packed)) {
    uint32_t t_sec;   /* seconds since 2000-01-01 00:00:00 */
    uint16_t t_ms;    /* milliseconds within second (0-999) */
    uint8_t  type;    /* REC_TYPE_* */
    uint8_t  chan;    /* ADC channel or GPIO pin index (0-5) */
    uint16_t value;   /* ADC counts (0-4095) or GPIO state (0/1) */
    uint8_t  seq;     /* rolling sequence counter */
    uint8_t  crc8;    /* CRC-8/MAXIM over bytes 0-10 */
} LogRecord;

/* ---- 32-byte file header ---- */
typedef struct __attribute__((packed)) {
    uint8_t  magic[4];     /* "SDLG" */
    uint8_t  version;      /* 1 */
    uint8_t  rec_size;     /* sizeof(LogRecord) = 12 */
    uint32_t epoch_base;   /* t_sec of first record */
    uint16_t session_id;   /* incremented each mount (from BKP_DR2) */
    uint8_t  flags;        /* bit0: 0=binary 1=CSV */
    uint16_t fw_ver;       /* firmware version BCD */
    uint8_t  reserved[15];
    uint16_t crc16;        /* CRC-16/CCITT over bytes 0-29 */
} LogHeader;

/* ---- CRC helpers ---- */
uint8_t  rec_crc8(const uint8_t *buf, size_t len);
uint16_t rec_crc16(const uint8_t *buf, size_t len);
uint32_t rec_crc32(const uint8_t *buf, size_t len);  /* CRC-32 IEEE */

/* ---- Record helpers ---- */
uint8_t rec_next_seq(void);
void    rec_seal(LogRecord *r);                   /* fill crc8 */
void    rec_fill_header(LogHeader *h, uint32_t epoch_base, uint16_t session, uint8_t is_csv);

/* ---- CSV formatter (returns chars written, excl. NUL) ---- */
int rec_to_csv(const LogRecord *r, char *buf, size_t buflen);

/* ---- Epoch helpers ---- */
uint32_t rtc_to_epoch(uint8_t year2k, uint8_t month, uint8_t day,
                       uint8_t hour,   uint8_t min,   uint8_t sec);

typedef struct { uint16_t year; uint8_t month, day, hour, min, sec; } DateTime;
void epoch_to_dt(uint32_t epoch, DateTime *dt);

#endif /* RECORDS_H */
