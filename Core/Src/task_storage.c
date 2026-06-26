#include "task_storage.h"
#include "datalogger.h"
#include "records.h"
#include "rtc_time.h"
#include "sd_spi.h"
#include "task_usb.h"
#include "board.h"
#include "ff.h"
#include "main.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>

/* ---- Globals ---- */
AppConfig g_config = {
    .sample_rate_ms = 1000,
    .log_format     = 0,     /* binary */
    .thr_lo         = 0,
    .thr_hi         = 4095,
    .oversample     = 1,
};

osMessageQueueId_t q_events;
osMessageQueueId_t q_cmd;
uint8_t g_streaming = 0;
uint8_t g_msc_enabled = 0;
osMutexId_t mtx_sd;

/* ---- Private ---- */
static FATFS    s_fatfs;
static FIL      s_log;
static uint16_t s_session;     /* incremented in BKP_DR2 each mount */
static int      s_log_open;
static int      s_logging = 1; /* runtime enable/disable */

static void reply(const char *msg) {
    usb_cdc_send_str(msg);
}

static void reply_ok(const char *body) {
    char buf[128];
    if (body && body[0]) {
        snprintf(buf, sizeof(buf), "OK %s\n", body);
    } else {
        snprintf(buf, sizeof(buf), "OK\n");
    }
    reply(buf);
}

static void reply_err(int code, const char *msg) {
    char buf[128];
    snprintf(buf, sizeof(buf), "ERR %d %s\n", code, msg ? msg : "error");
    reply(buf);
}

/* ---- CONFIG.TXT parser ---- */
static void load_config(void) {
    FIL  f;
    UINT br;
    char buf[128];

    if (f_open(&f, "0:/CONFIG.TXT", FA_READ) != FR_OK) return;
    f_read(&f, buf, sizeof(buf) - 1, &br);
    f_close(&f);
    buf[br] = '\0';

    char *p = buf;
    while (*p) {
        char *key = p;
        while (*p && *p != '=' && *p != '\r' && *p != '\n') p++;
        if (*p != '=') { while (*p && *p != '\n') p++; if (*p) p++; continue; }
        *p++ = '\0';
        char *val = p;
        while (*p && *p != '\r' && *p != '\n') p++;
        if (*p) { *p = '\0'; p++; }
        if (*p == '\n') p++;

        if (strcmp(key, "format") == 0) {
            g_config.log_format = (val[0] == 'c') ? 1u : 0u;
        } else if (strcmp(key, "rate") == 0) {
            uint32_t r = 0;
            for (char *c = val; *c >= '0' && *c <= '9'; c++) r = r * 10 + (uint32_t)(*c - '0');
            if (r >= 100 && r <= 60000) g_config.sample_rate_ms = r;
        } else if (strcmp(key, "oversample") == 0) {
            uint8_t n = (uint8_t)(val[0] - '0');
            if (n >= 1 && n <= 16) g_config.oversample = n;
        }
    }
}

/* ---- Open/create next log file ---- */
static FRESULT open_log(void) {
    /* Create /LOGS/ directory (ignore error if exists) */
    f_mkdir("0:/LOGS");

    char path[24];
    /* Find first available LOGnnnnn.BIN|CSV slot */
    for (uint16_t i = 1; i <= 99999; i++) {
        snprintf(path, sizeof(path),
                 g_config.log_format ? "0:/LOGS/L%05u.CSV" : "0:/LOGS/L%05u.BIN",
                 (unsigned)i);
        FILINFO fi;
        if (f_stat(path, &fi) != FR_OK) {  /* file does not exist */
            FRESULT fr = f_open(&s_log, path, FA_WRITE | FA_CREATE_NEW);
            if (fr != FR_OK) return fr;

            /* Write file header */
            Timestamp ts = rtc_get();

            if (!g_config.log_format) {
                /* Binary: 32-byte header */
                LogHeader hdr;
                rec_fill_header(&hdr, ts.sec, s_session, 0);
                UINT bw;
                f_write(&s_log, &hdr, sizeof(hdr), &bw);
            } else {
                /* CSV: column header */
                const char *csv_hdr = "timestamp,type,chan,value\n";
                UINT bw;
                f_write(&s_log, csv_hdr, strlen(csv_hdr), &bw);
            }
            f_sync(&s_log);
            return FR_OK;
        }
    }
    return FR_TOO_MANY_OPEN_FILES;
}

/* ---- Mount SD, load config, open log ---- */
static int mount_and_open(void) {
    if (!sd_spi_card_present()) return 0;
    /* f_mount with opt=1 calls disk_initialize → sd_spi_init internally */
    if (f_mount(&s_fatfs, "0:", 1) != FR_OK) return 0;

    /* Bump session counter (stored in BKP_DR2) */
    extern RTC_HandleTypeDef hrtc;
    s_session = (uint16_t)(HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR2) & 0xFFFF) + 1;
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR2, s_session);

    load_config();

    if (open_log() != FR_OK) {
        f_mount(NULL, "0:", 0);
        return 0;
    }

    s_log_open = 1;
    return 1;
}

/* ---- Write one record to the open log ---- */
static void write_record(const LogRecord *r) {
    UINT bw;
    if (!g_config.log_format) {
        f_write(&s_log, r, sizeof(*r), &bw);
    } else {
        char csv[80];
        int  len = rec_to_csv(r, csv, sizeof(csv));
        if (len > 0) f_write(&s_log, csv, (UINT)len, &bw);
    }

    if (g_streaming) {
        char csv[80];
        int len = rec_to_csv(r, csv, sizeof(csv));
        if (len > 0) {
            usb_cdc_send_str("#");
            usb_cdc_send_str(csv);
        }
    }
}

/* ---- Save CONFIG.TXT from current g_config ---- */
static FRESULT save_config(void) {
    FIL f;
    FRESULT fr = f_open(&f, "0:/CONFIG.TXT", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) return fr;

    char buf[128];
    int n = snprintf(buf, sizeof(buf),
                     "format=%s\nrate=%lu\noversample=%u\n",
                     g_config.log_format ? "csv" : "bin",
                     (unsigned long)g_config.sample_rate_ms,
                     (unsigned)g_config.oversample);
    UINT bw;
    f_write(&f, buf, (UINT)n, &bw);
    f_close(&f);
    return FR_OK;
}

/* ---- List files in a path ---- */
static void cmd_ls(const char *path) {
    DIR dir;
    FILINFO fi;
    const char *p = (path && path[0]) ? path : "0:/";

    if (f_opendir(&dir, p) != FR_OK) {
        reply_err(5, "cannot open dir");
        return;
    }

    char line[96];
    while (f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        snprintf(line, sizeof(line), "FILE %s %lu %04u-%02u-%02uT%02u:%02u:%02u\n",
                 fi.fname,
                 (unsigned long)fi.fsize,
                 (unsigned)((fi.fdate >> 9) + 1980),
                 (unsigned)((fi.fdate >> 5) & 0x0F),
                 (unsigned)(fi.fdate & 0x1F),
                 (unsigned)((fi.ftime >> 11) & 0x1F),
                 (unsigned)((fi.ftime >> 5) & 0x3F),
                 (unsigned)((fi.ftime & 0x1F) << 1));
        reply(line);
    }
    f_closedir(&dir);
    reply_ok("");
}

/* ---- Download file in framed chunks ---- */
#define DL_PAYLOAD_MAX 64
#define DL_SOF 0x7E

static void cmd_get(const char *name) {
    char path[40];
    if (name[0] == '0' && name[1] == ':') {
        snprintf(path, sizeof(path), "%s", name);
    } else {
        snprintf(path, sizeof(path), "0:/%s", name);
    }

    FIL f;
    FRESULT fr = f_open(&f, path, FA_READ);
    if (fr != FR_OK) {
        reply_err((fr == FR_NO_FILE || fr == FR_NO_PATH) ? 3 : 5, "open failed");
        return;
    }

    uint32_t fsize = f_size(&f);
    uint8_t  buf[DL_PAYLOAD_MAX];
    uint32_t crc32 = 0xFFFFFFFFU;
    UINT     br;

    /* CRC-32/IEEE over the whole file (single pass) */
    while (f_read(&f, buf, sizeof(buf), &br) == FR_OK && br > 0) {
        for (size_t i = 0; i < br; i++) {
            crc32 ^= (uint32_t)buf[i] << 24;
            for (int j = 0; j < 8; j++) {
                crc32 = (crc32 & 0x80000000U) ? ((crc32 << 1) ^ 0x04C11DB7U) : (crc32 << 1);
            }
        }
    }
    crc32 ^= 0xFFFFFFFFU;
    f_lseek(&f, 0);

    char hdr[48];
    snprintf(hdr, sizeof(hdr), "OK %lu %08lX\n", (unsigned long)fsize, (unsigned long)crc32);
    reply(hdr);

    uint32_t seq = 0;
    while (f_read(&f, buf, sizeof(buf), &br) == FR_OK && br > 0) {
        uint16_t crc16 = rec_crc16(buf, br);

        /* Frame layout: SOF(1) + seq(4) + len(2) + payload + crc16(2) */
        uint8_t frame[1 + 4 + 2 + DL_PAYLOAD_MAX + 2];
        uint32_t idx = 0;
        frame[idx++] = DL_SOF;
        frame[idx++] = (uint8_t)(seq);
        frame[idx++] = (uint8_t)(seq >> 8);
        frame[idx++] = (uint8_t)(seq >> 16);
        frame[idx++] = (uint8_t)(seq >> 24);
        frame[idx++] = (uint8_t)(br);
        frame[idx++] = (uint8_t)(br >> 8);
        memcpy(&frame[idx], buf, br);
        idx += br;
        frame[idx++] = (uint8_t)(crc16 >> 8);
        frame[idx++] = (uint8_t)(crc16);

        usb_cdc_send_buf(frame, idx);
        seq++;
    }

    f_close(&f);

    /* End-of-transfer frame: len = 0 */
    uint8_t eof_frame[7] = { DL_SOF, 0, 0, 0, 0, 0, 0 };
    eof_frame[1] = (uint8_t)(seq);
    eof_frame[2] = (uint8_t)(seq >> 8);
    eof_frame[3] = (uint8_t)(seq >> 16);
    eof_frame[4] = (uint8_t)(seq >> 24);
    usb_cdc_send_buf(eof_frame, sizeof(eof_frame));

    reply("DONE\n");
}

/* ---- Delete a file ---- */
static void cmd_del(const char *name) {
    char path[40];
    if (name[0] == '0' && name[1] == ':') {
        snprintf(path, sizeof(path), "%s", name);
    } else {
        snprintf(path, sizeof(path), "0:/%s", name);
    }
    FRESULT fr = f_unlink(path);
    if (fr == FR_OK) {
        reply_ok("");
    } else if (fr == FR_NO_FILE || fr == FR_NO_PATH) {
        reply_err(3, "not found");
    } else {
        reply_err(5, "io error");
    }
}

/* ---- Format the volume (guard token required) ---- */
static void cmd_format(const char *token) {
    if (strcmp(token, "YES") != 0) {
        reply_err(1, "format YES");
        return;
    }
    /* Close active log before formatting */
    if (s_log_open) {
        f_close(&s_log);
        s_log_open = 0;
    }
    FRESULT fr = f_mkfs("0:", 0, 0);  /* R0.11 API: auto FAT type */
    if (fr != FR_OK) {
        reply_err(5, "format failed");
        return;
    }
    /* Re-mount and create default CONFIG.TXT */
    if (f_mount(&s_fatfs, "0:", 1) != FR_OK) {
        reply_err(5, "remount failed");
        return;
    }
    g_config.log_format = 0;
    g_config.sample_rate_ms = 1000;
    g_config.oversample = 1;
    save_config();
    s_log_open = (open_log() == FR_OK) ? 1 : 0;
    reply_ok("");
}

/* ---- Handle one command from proto task ---- */
static void handle_cmd(const StorageCmd *c) {
    switch (c->cmd) {
    case SCMD_LOG_START:
        s_logging = 1;
        reply_ok("");
        break;
    case SCMD_LOG_STOP:
        s_logging = 0;
        if (s_log_open) f_sync(&s_log);
        reply_ok("");
        break;
    case SCMD_MSC_ON:
        if (s_log_open) {
            f_close(&s_log);
            f_mount(NULL, "0:", 0);
            s_log_open = 0;
        }
        g_msc_enabled = 1;
        reply_ok("");
        break;
    case SCMD_MSC_OFF:
        g_msc_enabled = 0;
        mount_and_open();
        reply_ok("");
        break;
    case SCMD_LS:
        cmd_ls(c->arg);
        break;
    case SCMD_DEL:
        cmd_del(c->arg);
        break;
    case SCMD_FORMAT:
        cmd_format(c->arg);
        break;
    case SCMD_CFG_GET:
        {
            char buf[80];
            snprintf(buf, sizeof(buf),
                     "format=%s rate=%lu oversample=%u",
                     g_config.log_format ? "csv" : "bin",
                     (unsigned long)g_config.sample_rate_ms,
                     (unsigned)g_config.oversample);
            reply_ok(buf);
        }
        break;
    case SCMD_CFG_SET:
        {
            char *p = (char *)c->arg;
            char *key = p;
            while (*p && *p != '=') p++;
            if (*p != '=') { reply_err(1, "cfg key=value"); break; }
            *p++ = '\0';
            char *val = p;
            if (strcmp(key, "format") == 0) {
                g_config.log_format = (val[0] == 'c') ? 1u : 0u;
            } else if (strcmp(key, "rate") == 0) {
                uint32_t r = 0;
                for (char *c2 = val; *c2 >= '0' && *c2 <= '9'; c2++) r = r * 10 + (uint32_t)(*c2 - '0');
                if (r >= 100 && r <= 60000) g_config.sample_rate_ms = r;
            } else if (strcmp(key, "oversample") == 0) {
                uint8_t n = (uint8_t)(val[0] - '0');
                if (n >= 1 && n <= 16) g_config.oversample = n;
            } else {
                reply_err(1, "unknown key");
                break;
            }
            save_config();
            reply_ok("");
        }
        break;
    case SCMD_GET:
        cmd_get(c->arg);
        break;
    default:
        reply_err(1, "unknown cmd");
        break;
    }
}

/* ---- Storage task main loop ---- */
void task_storage_start(void *arg) {
    (void)arg;

    uint32_t last_sync = osKernelGetTickCount();
    uint32_t last_led  = last_sync;

    /* Try to mount once. The main loop below keeps retrying the mount while
       also servicing the command queue, so an unmountable card (no FAT / GPT /
       exFAT, or one that needs FORMAT) does NOT block the USB command channel. */
    board_led_write(LED_ERR, mount_and_open() ? 0 : 1);

    for (;;) {
        /* Process pending file commands (non-blocking) */
        StorageCmd cmd;
        if (osMessageQueueGet(q_cmd, &cmd, NULL, 0) == osOK) {
            handle_cmd(&cmd);
        }

        LogRecord rec;
        /* Wait up to 200 ms for a record */
        osStatus_t st = osMessageQueueGet(q_events, &rec, NULL, 200);

        if (st == osOK) {
            if (s_log_open && s_logging) write_record(&rec);
        }

        uint32_t now = osKernelGetTickCount();

        /* f_sync every 5 s */
        if (s_log_open && (now - last_sync) >= 5000) {
            f_sync(&s_log);
            last_sync = now;
        }

        /* LED indicators */
        board_led_write(LED_CARD, sd_spi_card_present() ? 1 : 0);
        if (s_log_open && s_logging && (now - last_led) >= 500) {
            board_led_toggle(LED_LOG);
            last_led = now;
        } else if (!s_logging) {
            board_led_write(LED_LOG, 0);
        }

        /* Check for card removal */
        if (s_log_open && !sd_spi_card_present()) {
            f_sync(&s_log);
            f_close(&s_log);
            f_mount(NULL, "0:", 0);
            s_log_open = 0;
            board_led_write(LED_LOG, 0);
        }

        /* Try re-mount if card was removed then reinserted.
           Skip while MSC is active: the host owns the sectors; re-initialising
           the SPI card state (CMD0 inside sd_spi_init) would corrupt an
           in-flight MSC transfer and defeat the exclusive-access guarantee. */
        if (!s_log_open && !g_msc_enabled) {
            if (mount_and_open()) {
                board_led_write(LED_ERR, 0);
            } else {
                board_led_write(LED_ERR, 1);
            }
        }
    }
}
