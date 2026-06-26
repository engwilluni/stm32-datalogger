/*
 * board.h — Board-level helpers for STM32F107 datalogger.
 */

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- LEDs (PE8-PE15) ---- */
typedef enum {
    LED_LOG = 0,      /* PE8  - blink while logging */
    LED_ERR,          /* PE9  - error */
    LED_CARD,         /* PE10 - card present/mounted */
    LED_USB,          /* PE11 - USB mounted */
    LED_STREAM,       /* PE12 - streaming active */
    LED_ALARM,        /* PE13 - threshold alarm */
    LED_RES1,         /* PE14 */
    LED_RES2,         /* PE15 */
    LED_COUNT
} LedId;

void board_led_init(void);
void board_led_write(LedId led, int on);
void board_led_toggle(LedId led);
void board_led_all_off(void);

/* ---- USB ---- */
void board_usb_init(void);
void board_usb_get_serial_str(char *buf, size_t buflen); /* hex UID + NUL */
size_t board_usb_get_serial(uint16_t *utf16_buf, size_t max_chars);

/* ---- SPI speed ---- */
void board_spi_set_speed_fast(void);
void board_spi_set_speed_slow(void);

/* ---- IWDG ---- */
void board_iwdg_init(void);
void board_iwdg_refresh(void);

/* ---- UID ---- */
void board_get_uid(uint32_t uid[3]);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
