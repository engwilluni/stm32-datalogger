/*
 * ffconf.h — FatFs R0.11 configuration for STM32F107 datalogger
 *
 * Design choices:
 *  - FF_FS_TINY=1 : single 512 B sector window (saves RAM vs per-file buffer)
 *  - No LFN       : 8.3 names only (saves ~3 KB RAM/code)
 *  - No reentrant : single-owner (only storage task calls FatFs)
 */

#ifndef _FFCONF
#define _FFCONF 32020  /* Revision ID for R0.11 */

#include "stm32f1xx_hal.h"

/* ---------- Functions and buffer size ---------- */
#define _FS_TINY         1   /* 1 = tiny: shared 512 B window, no per-FIL buffer */
#define _FS_READONLY     0
#define _FS_MINIMIZE     0
#define _USE_STRFUNC     0   /* disable f_puts/f_printf (use integer-only formatting) */
#define _USE_FIND        0
#define _USE_MKFS        1   /* enable f_mkfs() for FORMAT command */
#define _USE_FASTSEEK    0
#define _USE_LABEL       0
#define _USE_FORWARD     0

/* ---------- Locale ---------- */
#define _CODE_PAGE       437  /* ASCII only (no extended chars for 8.3 names) */
#define _USE_LFN         0    /* 0 = no LFN; saves ~3 KB Flash and stack */
#define _MAX_LFN         255
#define _LFN_UNICODE     0
#define _STRF_ENCODE     3
#define _FS_RPATH        0

/* ---------- Drive / volume ---------- */
#define _VOLUMES         1
#define _STR_VOLUME_ID   0
#define _MULTI_PARTITION 0
#define _MIN_SS          512
#define _MAX_SS          512   /* fixed 512-byte sectors for SD SPI */
#define _USE_TRIM        0
#define _FS_NOFSINFO     0

/* ---------- System ---------- */
#define _FS_NORTC        0    /* 0 = use get_fattime() from RTC */
#define _NORTC_MON       1
#define _NORTC_MDAY      1
#define _NORTC_YEAR      2026

#define _FS_LOCK         0    /* 0 = no open-file locking (single-owner design) */

#define _FS_REENTRANT    0    /* 0 = not re-entrant (single storage task owns FatFs) */
#define _FS_TIMEOUT      1000
#define _SYNC_t          osSemaphoreId

#define _WORD_ACCESS     0    /* 0 = always byte-by-byte (safe for Cortex-M3) */

#endif /* _FFCONF */
