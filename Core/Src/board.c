/*
 * board.c — Board-level helpers for STM32F107 datalogger.
 */

#include "board.h"
#include "main.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

extern SPI_HandleTypeDef hspi1;

/* ---- LED pin table (PE8-PE15) ---- */
static const uint16_t s_led_pins[LED_COUNT] = {
    GPIO_PIN_8,  GPIO_PIN_9,  GPIO_PIN_10, GPIO_PIN_11,
    GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15
};

void board_led_init(void) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
               | GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);
    board_led_all_off();
}

void board_led_write(LedId led, int on) {
    if (led >= LED_COUNT) return;
    HAL_GPIO_WritePin(GPIOE, s_led_pins[led], on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_led_toggle(LedId led) {
    if (led >= LED_COUNT) return;
    HAL_GPIO_TogglePin(GPIOE, s_led_pins[led]);
}

void board_led_all_off(void) {
    for (int i = 0; i < LED_COUNT; i++) {
        board_led_write((LedId)i, 0);
    }
}

/* ---- SPI speed ---- */
void board_spi_set_speed_slow(void) {
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_256);
    __HAL_SPI_ENABLE(&hspi1);
}

void board_spi_set_speed_fast(void) {
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_4);
    __HAL_SPI_ENABLE(&hspi1);
}

/* ---- UID ---- */
void board_get_uid(uint32_t uid[3]) {
    uid[0] = HAL_GetUIDw0();
    uid[1] = HAL_GetUIDw1();
    uid[2] = HAL_GetUIDw2();
}

void board_usb_get_serial_str(char *buf, size_t buflen) {
    uint32_t uid[3];
    board_get_uid(uid);
    if (buflen > 0) {
        snprintf(buf, buflen,
                 "%08X%08X%08X",
                 (unsigned)uid[0], (unsigned)uid[1], (unsigned)uid[2]);
    }
}

size_t board_usb_get_serial(uint16_t *utf16_buf, size_t max_chars) {
    char serial[32];
    board_usb_get_serial_str(serial, sizeof(serial));
    size_t n = strlen(serial);
    if (n > max_chars) n = max_chars;
    for (size_t i = 0; i < n; i++) {
        utf16_buf[i] = (uint16_t)serial[i];
    }
    return n;
}

/* ---- IWDG ---- */
void board_iwdg_init(void) {
    IWDG->KR  = 0x5555;        /* enable register access */
    IWDG->PR  = 4;             /* prescaler /64 */
    IWDG->RLR = 2500;          /* ~4 s timeout @ 40 kHz LSI */
    IWDG->KR  = 0xAAAA;        /* reload */
    IWDG->KR  = 0xCCCC;        /* start watchdog */
}

void board_iwdg_refresh(void) {
    IWDG->KR = 0xAAAA;
}

/* ---- USB hardware init ---- */
void board_usb_init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

    /* On STM32F107 the OTG_FS full-speed PHY is hard-wired to PA11/PA12
       (no separate AF/GPIO config needed once the OTG clock is on).
       This board routes the cable VBUS to PA9 (confirmed: PA9 reads high when
       plugged). The early F105/107 dwc2 core does NOT implement the GOTGCTL
       B-valid override (BVALOEN/BVALOVAL) that TinyUSB writes to bypass VBUS
       sensing, so the device's session-valid (GOTGCTL.BSVLD) comes ONLY from
       the hardware VBUS comparator. We must therefore ENABLE B-device VBUS
       sensing — with it disabled the core never sees a session and never
       enumerates. (This is re-asserted after tusb_init() in task_usb.) */
    USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBUSASEN;     /* not an A-device (host) */
    USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_PWRDWN         /* power up embedded FS PHY */
                      |  USB_OTG_GCCFG_VBUSBSEN;       /* sense VBUS on PA9 -> B-session valid */

    /* NVIC: priority 6 (within FreeRTOS syscall range 5-15).
       The DWC2 driver will enable the IRQ during tusb_init(); we set
       the priority here so it is correct once enabled. */
    HAL_NVIC_SetPriority(OTG_FS_IRQn, 6, 0);
}
