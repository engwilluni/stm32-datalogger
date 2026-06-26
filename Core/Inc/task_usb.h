/*
 * task_usb.h — TinyUSB device task.
 */

#ifndef TASK_USB_H
#define TASK_USB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void task_usb_start(void *arg);

/* Return non-zero if USB is mounted (configured) */
int usb_mounted(void);

/* Helpers used by proto.c */
void usb_cdc_send_str(const char *str);
void usb_cdc_send_buf(const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* TASK_USB_H */
