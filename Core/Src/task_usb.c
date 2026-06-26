/*
 * task_usb.c — TinyUSB device task and CDC helpers.
 */

#include "task_usb.h"
#include "board.h"
#include "tusb.h"
#include "cmsis_os.h"
#include "main.h"   /* USB_OTG_FS register access for the VBUS-sense fix */

static volatile int s_mounted = 0;

/* ----------------------------------------------------------------
 * TinyUSB callbacks
 * ---------------------------------------------------------------- */
void tud_mount_cb(void) {
    s_mounted = 1;
    board_led_write(LED_USB, 1);
}

void tud_umount_cb(void) {
    s_mounted = 0;
    board_led_write(LED_USB, 0);
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    board_led_write(LED_USB, 0);
}

void tud_resume_cb(void) {
    if (tud_mounted()) {
        board_led_write(LED_USB, 1);
    }
}

int usb_mounted(void) {
    return s_mounted;
}

/* ----------------------------------------------------------------
 * CDC helpers
 * ---------------------------------------------------------------- */
void usb_cdc_send_str(const char *str) {
    if (!tud_cdc_connected()) return;
    uint32_t len = (uint32_t)strlen(str);
    uint32_t sent = 0;
    while (sent < len) {
        uint32_t chunk = tud_cdc_write(str + sent, len - sent);
        if (chunk == 0) {
            tud_cdc_write_flush();
            osDelay(1);
        }
        sent += chunk;
    }
    tud_cdc_write_flush();
}

void usb_cdc_send_buf(const uint8_t *buf, uint32_t len) {
    if (!tud_cdc_connected()) return;
    uint32_t sent = 0;
    while (sent < len) {
        uint32_t chunk = tud_cdc_write(buf + sent, len - sent);
        if (chunk == 0) {
            tud_cdc_write_flush();
            osDelay(1);
        }
        sent += chunk;
    }
    tud_cdc_write_flush();
}

/* ----------------------------------------------------------------
 * MSC callbacks
 * ---------------------------------------------------------------- */
#include "tusb.h"
#include "datalogger.h"
#include "sd_spi.h"
#include "board.h"

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;
    const char vid[] = "OpenCode";
    const char pid[] = "Datalogger      ";
    const char rev[] = "1.0 ";
    memcpy(vendor_id,  vid, 8);
    memcpy(product_id, pid, 16);
    memcpy(product_rev, rev, 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void) lun;
    return g_msc_enabled && sd_spi_card_present();
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void) lun;
    *block_count = sd_spi_get_sector_count();
    *block_size  = 512;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void) lun; (void) power_condition; (void) start; (void) load_eject;
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void) lun;
    if (!g_msc_enabled || bufsize != 512 || offset != 0) return -1;
    if (sd_spi_read(lba, buffer, 1) != 0) return -1;
    return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    (void) lun;
    if (!g_msc_enabled || bufsize != 512 || offset != 0) return -1;
    if (sd_spi_write(lba, buffer, 1) != 0) return -1;
    return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize) {
    (void) lun; (void) scsi_cmd; (void) buffer; (void) bufsize;
    return 0;
}

/* ----------------------------------------------------------------
 * Device task
 * ---------------------------------------------------------------- */
void task_usb_start(void *arg) {
    (void) arg;

    board_led_write(LED_RES1, 1);  /* D6: entered USB task */
    tusb_init();
    /* F105/107 fix: TinyUSB's dcd_init forces B-valid via GOTGCTL override
       bits that this old dwc2 core ignores, so enumeration depends on real
       VBUS sensing. Re-assert VBUSBSEN here (after all TinyUSB init) so the
       core sees the session on PA9 VBUS and connects. Verified on hardware:
       without this the host never detects the device (BSVLD stays 0). */
    USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBUSBSEN;
    board_led_write(LED_RES2, 1);  /* D7: tusb_init done */

    for (;;) {
        tud_task();
        tud_cdc_write_flush();
        osDelay(1);
    }
}
