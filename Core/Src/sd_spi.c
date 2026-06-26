/*
 * sd_spi.c — Low-level SPI SD card driver for STM32F107 datalogger
 *
 * Hardware: SPI1 (APB2 72 MHz), CS=PA4, SCK=PA5, MISO=PA6, MOSI=PA7
 * SPI speed: /256 = 281 kHz during CMD0/ACMD41 init; /4 = 18 MHz thereafter.
 *
 * Protocol follows ChaN's mmc_stm32 reference sequence:
 *   - 74+ idle clocks (CS high) before CMD0
 *   - CMD8 probe to distinguish SD v1.x / v2.0 (SDHC)
 *   - CMD17/18 for read, CMD24/25 for write
 *   - Block addressing for SDHC; byte addressing (sector*512) for SDSC
 */

#include "sd_spi.h"
#include "datalogger.h"
#include "main.h"

extern SPI_HandleTypeDef hspi1;

/* Helper: recursive mutex around all SPI/SD operations */
static void sd_lock(void) {
    if (mtx_sd) osMutexAcquire(mtx_sd, osWaitForever);
}

static void sd_unlock(void) {
    if (mtx_sd) osMutexRelease(mtx_sd);
}

/* ---------- CS control ---------- */
#define CS_LOW()   HAL_GPIO_WritePin(SDCARD_D3_GPIO_Port, SDCARD_D3_Pin, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(SDCARD_D3_GPIO_Port, SDCARD_D3_Pin, GPIO_PIN_SET)

/* ---------- SPI speed (write BR field in CR1) ---------- */
static void spi_speed(uint32_t br) {
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, br);
    __HAL_SPI_ENABLE(&hspi1);
}

/* ---------- Low-level byte exchange ---------- */
static uint8_t spi_byte(uint8_t b) {
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi1, &b, &rx, 1, 10);
    return rx;
}

/* Wait until MISO goes high (card releases DO) */
static int sd_wait_ready(uint32_t timeout_ms) {
    uint32_t deadline = HAL_GetTick() + timeout_ms;
    while (spi_byte(0xFF) != 0xFF) {
        if (HAL_GetTick() > deadline) return 0;
    }
    return 1;
}

/* ---------- Send SPI SD command, return R1 byte ---------- */
static uint8_t sd_cmd(uint8_t cmd, uint32_t arg) {
    /* Deselect-then-reselect pattern: releases card from previous response */
    CS_HIGH();
    spi_byte(0xFF);
    CS_LOW();

    sd_wait_ready(500);

    uint8_t frame[6];
    frame[0] = cmd | 0x40;
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >>  8);
    frame[4] = (uint8_t)(arg);
    /* Valid CRC required only for CMD0 (0x95) and CMD8 (0x87) in SPI mode */
    if      (cmd ==  0) frame[5] = 0x95;
    else if (cmd ==  8) frame[5] = 0x87;
    else                frame[5] = 0x01;  /* dummy stop bit */

    HAL_SPI_Transmit(&hspi1, frame, 6, 100);

    /* CMD12 (stop transmission) has an extra skip byte */
    if (cmd == 12) spi_byte(0xFF);

    /* R1 response: MSB=0 when valid; poll up to 10 bytes */
    uint8_t r1 = 0xFF;
    for (int i = 0; i < 10; i++) {
        r1 = spi_byte(0xFF);
        if (!(r1 & 0x80)) break;
    }
    return r1;
}

/* ---------- Receive a data block of `len` bytes from card ----------
 * len is 512 for a sector (CMD17/18) or 16 for the CSD/CID (CMD9/10).
 * IMPORTANT: never transfer more than the caller's buffer holds — passing a
 * fixed 512 here once overran the 16-byte CSD buffer in get_sector_count. */
static int sd_recv_block(uint8_t *buf, uint32_t len) {
    /* Wait for data token 0xFE (up to 200 ms) */
    uint32_t deadline = HAL_GetTick() + 200;
    uint8_t tok;
    do {
        tok = spi_byte(0xFF);
        if (HAL_GetTick() > deadline) return 0;
    } while (tok == 0xFF);

    if (tok != 0xFE) return 0;  /* data error token instead */

    /* Receive `len` data bytes: fill buf with 0xFF (dummy TX), then transceive */
    for (uint32_t i = 0; i < len; i++) buf[i] = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, buf, buf, (uint16_t)len, 5000);

    /* Discard 2-byte CRC */
    spi_byte(0xFF);
    spi_byte(0xFF);
    return 1;
}

/* ---------- Send a 512-byte data block to card ---------- */
static int sd_send_block(const uint8_t *buf, uint8_t token) {
    if (!sd_wait_ready(500)) return 0;

    spi_byte(token);

    if (token == 0xFD) {  /* stop-transmission token: no data follows */
        spi_byte(0xFF);
        sd_wait_ready(500);
        return 1;
    }

    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, 512, 5000);
    spi_byte(0xFF);  /* dummy CRC high */
    spi_byte(0xFF);  /* dummy CRC low  */

    uint8_t resp = spi_byte(0xFF) & 0x1F;
    if (resp != 0x05) return 0;  /* data rejected */

    return sd_wait_ready(500);   /* wait for write completion */
}

/* ---------- Card type flags ---------- */
static uint8_t s_card_type = 0;
#define CT_SD1    0x01  /* SD v1.x      */
#define CT_SD2    0x02  /* SD v2.0+     */
#define CT_BLOCK  0x04  /* SDHC/SDXC (block-addressed) */

/* ----------------------------------------------------------------
 * sd_spi_init — Power-on initialisation sequence
 * Returns 0 on success, -1 on error.
 * ---------------------------------------------------------------- */
int sd_spi_init(void) {
    sd_lock();
    s_card_type = 0;

    spi_speed(SPI_BAUDRATEPRESCALER_256);   /* 72 MHz / 256 = 281 kHz ≤ 400 kHz */

    /* ≥74 idle clocks with CS high to trigger SPI mode entry */
    CS_HIGH();
    for (int i = 0; i < 10; i++) spi_byte(0xFF);

    /* CMD0: GO_IDLE_STATE → R1 must be 0x01 (idle) */
    uint8_t r1 = 0xFF;
    for (int retry = 0; retry < 10; retry++) {
        r1 = sd_cmd(0, 0);
        if (r1 == 0x01) break;
    }
    if (r1 != 0x01) { CS_HIGH(); spi_byte(0xFF); sd_unlock(); return -1; }

    /* CMD8: SEND_IF_COND — distinguishes SD v1.x from v2.0 */
    uint8_t ocr[4];
    uint8_t ctype;

    r1 = sd_cmd(8, 0x1AA);
    if (r1 == 0x01) {
        /* v2.0 card: read 4-byte R7 response */
        for (int i = 0; i < 4; i++) ocr[i] = spi_byte(0xFF);

        if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
            /* Voltage accepted; send ACMD41(HCS=1) until card ready */
            ctype = CT_SD2;
            uint32_t deadline = HAL_GetTick() + 1000;
            do {
                sd_cmd(55, 0);                         /* APP_CMD */
                r1 = sd_cmd(41, 0x40000000);           /* SD_SEND_OP_COND with HCS */
                if (HAL_GetTick() > deadline) { ctype = 0; break; }
            } while (r1 == 0x01);

            if (ctype && r1 == 0x00) {
                /* CMD58: read OCR to test CCS bit (SDHC indicator) */
                sd_cmd(58, 0);
                for (int i = 0; i < 4; i++) ocr[i] = spi_byte(0xFF);
                if (ocr[0] & 0x40) ctype |= CT_BLOCK;  /* SDHC/SDXC */
            }
        } else {
            ctype = 0;
        }
    } else {
        /* v1.x card: no CMD8 response; send ACMD41 without HCS */
        ctype = CT_SD1;
        uint32_t deadline = HAL_GetTick() + 1000;
        do {
            sd_cmd(55, 0);
            r1 = sd_cmd(41, 0);
            if (HAL_GetTick() > deadline) { ctype = 0; break; }
        } while (r1 == 0x01);
    }

    CS_HIGH();
    spi_byte(0xFF);

    if (!ctype) { sd_unlock(); return -1; }

    s_card_type = ctype;
    spi_speed(SPI_BAUDRATEPRESCALER_4);     /* 72 MHz / 4 = 18 MHz */
    sd_unlock();
    return 0;
}

/* ----------------------------------------------------------------
 * sd_spi_card_present — Read card-detect switch (PE0, active-low)
 * ---------------------------------------------------------------- */
int sd_spi_card_present(void) {
    return HAL_GPIO_ReadPin(SDCARD_CD_GPIO_Port, SDCARD_CD_Pin) == GPIO_PIN_RESET;
}


/* ----------------------------------------------------------------
 * sd_spi_get_sector_count — Parse CSD to compute total sectors
 * ---------------------------------------------------------------- */
uint32_t sd_spi_get_sector_count(void) {
    if (!s_card_type) return 0;

    sd_lock();
    uint8_t csd[16];
    uint32_t count = 0;

    CS_LOW();
    if (sd_cmd(9, 0) == 0 && sd_recv_block(csd, 16)) {
        if ((csd[0] >> 6) == 1) {
            /* CSD v2.0 (SDHC/SDXC): C_SIZE in bytes 7-9 */
            uint32_t csize = ((uint32_t)(csd[7] & 0x3F) << 16)
                           | ((uint32_t)csd[8] << 8)
                           |  (uint32_t)csd[9];
            count = (csize + 1) * 1024;
        } else {
            /* CSD v1.0 (SDSC) */
            uint8_t n = (csd[5] & 0x0F)
                      + ((csd[10] & 0x80) >> 7)
                      + ((csd[9]  &  0x03) << 1)
                      + 2;
            uint32_t csize = ((uint32_t)(csd[6] & 0x03) << 10)
                           | ((uint32_t)csd[7] << 2)
                           | ((uint32_t)(csd[8] & 0xC0) >> 6);
            count = (csize + 1) << (n - 9);
        }
    }
    CS_HIGH();
    spi_byte(0xFF);
    sd_unlock();
    return count;
}

/* ----------------------------------------------------------------
 * sd_spi_read — Read `count` 512-byte sectors starting at `sector`
 * ---------------------------------------------------------------- */
int sd_spi_read(uint32_t sector, uint8_t *buf, uint32_t count) {
    if (!s_card_type || !count) return -1;

    sd_lock();
    /* SDSC: convert block address to byte address */
    if (!(s_card_type & CT_BLOCK)) sector *= 512;

    uint8_t cmd = (count == 1) ? 17 : 18;  /* READ_SINGLE / READ_MULTIPLE */
    int ret = 0;

    CS_LOW();
    if (sd_cmd(cmd, sector) == 0) {
        do {
            if (!sd_recv_block(buf, 512)) { ret = -1; break; }
            buf += 512;
        } while (--count);
        if (cmd == 18) sd_cmd(12, 0);       /* STOP_TRANSMISSION */
    } else {
        ret = -1;
    }
    CS_HIGH();
    spi_byte(0xFF);
    sd_unlock();
    return ret;
}

/* ----------------------------------------------------------------
 * sd_spi_write — Write `count` 512-byte sectors starting at `sector`
 * ---------------------------------------------------------------- */
int sd_spi_write(uint32_t sector, const uint8_t *buf, uint32_t count) {
    if (!s_card_type || !count) return -1;

    sd_lock();
    if (!(s_card_type & CT_BLOCK)) sector *= 512;

    int ret = 0;
    CS_LOW();

    if (count == 1) {
        /* CMD24: WRITE_BLOCK */
        if (sd_cmd(24, sector) == 0) {
            if (!sd_send_block(buf, 0xFE)) ret = -1;
        } else {
            ret = -1;
        }
    } else {
        /* CMD25: WRITE_MULTIPLE_BLOCK */
        if (sd_cmd(25, sector) == 0) {
            do {
                if (!sd_send_block(buf, 0xFC)) { ret = -1; break; }
                buf += 512;
            } while (--count);
            sd_send_block(NULL, 0xFD);      /* stop transmission token */
        } else {
            ret = -1;
        }
    }

    CS_HIGH();
    spi_byte(0xFF);
    sd_unlock();
    return ret;
}
