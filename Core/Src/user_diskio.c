/*
 * user_diskio.c — FatFs diskio adapter for SD card via SPI
 *
 * Implements ChaN FatFs R0.11 diskio interface.
 * Only volume 0 is used (SD card on SPI1).
 * get_fattime() reads the RTC (LSE, ~30 µs resolution).
 */

#include "diskio.h"  /* includes integer.h for BYTE/WORD/DWORD/UINT; defines DSTATUS/DRESULT */
#include "sd_spi.h"
#include "main.h"

extern RTC_HandleTypeDef hrtc;

static volatile DSTATUS s_stat = STA_NOINIT;

/* ----------------------------------------------------------------
 * disk_initialize — Power-on and init the SD card
 * ---------------------------------------------------------------- */
DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;

    if (!sd_spi_card_present()) {
        s_stat = STA_NOINIT | STA_NODISK;
        return s_stat;
    }

    s_stat = STA_NOINIT;
    if (sd_spi_init() == 0) {
        s_stat = 0;
    }
    return s_stat;
}

/* ----------------------------------------------------------------
 * disk_status — Return current disk status
 * ---------------------------------------------------------------- */
DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    if (!sd_spi_card_present()) s_stat |= STA_NODISK;
    return s_stat;
}

/* ----------------------------------------------------------------
 * disk_read — Read sector(s)
 * ---------------------------------------------------------------- */
DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
    if (pdrv != 0 || !count) return RES_PARERR;
    if (s_stat & STA_NOINIT) return RES_NOTRDY;
    return (sd_spi_read(sector, buff, count) == 0) ? RES_OK : RES_ERROR;
}

/* ----------------------------------------------------------------
 * disk_write — Write sector(s)
 * ---------------------------------------------------------------- */
#if !_FS_READONLY
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count) {
    if (pdrv != 0 || !count) return RES_PARERR;
    if (s_stat & STA_NOINIT) return RES_NOTRDY;
    return (sd_spi_write(sector, buff, count) == 0) ? RES_OK : RES_ERROR;
}
#endif

/* ----------------------------------------------------------------
 * disk_ioctl — Control functions
 * ---------------------------------------------------------------- */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0) return RES_PARERR;
    if (s_stat & STA_NOINIT) return RES_NOTRDY;

    switch (cmd) {
    case CTRL_SYNC:         /* flush pending writes (polling SPI, nothing to flush) */
        return RES_OK;

    case GET_SECTOR_COUNT: {
        uint32_t n = sd_spi_get_sector_count();
        if (!n) return RES_ERROR;
        *(DWORD *)buff = n;
        return RES_OK;
    }

    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        return RES_OK;

    case GET_BLOCK_SIZE:    /* erase block size in sectors; 1 = unknown */
        *(DWORD *)buff = 1;
        return RES_OK;

    default:
        return RES_PARERR;
    }
}

/* ----------------------------------------------------------------
 * get_fattime — Provide current time for file timestamps
 *
 * FatFs format: bits [31:25] year from 1980, [24:21] month,
 *               [20:16] day, [15:11] hour, [10:5] min, [4:0] sec/2
 *
 * STM32F1 quirk: date is in BKP registers emulated by HAL; read
 * Time before Date to avoid any latching race.
 * ---------------------------------------------------------------- */
DWORD get_fattime(void) {
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    /* d.Year is 0-99 (offset from 2000); FatFs epoch is 1980 → add 20 */
    return ((DWORD)(d.Year + 20) << 25)
         | ((DWORD)d.Month       << 21)
         | ((DWORD)d.Date        << 16)
         | ((DWORD)t.Hours       << 11)
         | ((DWORD)t.Minutes     <<  5)
         | ((DWORD)t.Seconds     >>  1);
}
