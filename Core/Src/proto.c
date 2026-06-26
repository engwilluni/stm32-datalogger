/*
 * proto.c — USB-CDC command parser and dispatcher.
 *
 * Protocol: one ASCII line per command, terminated by '\n'.
 * Response: "OK ...\n" or "ERR <code> <msg>\n".
 */

#include "proto.h"
#include "task_usb.h"
#include "datalogger.h"
#include "rtc_time.h"
#include "records.h"
#include "sd_spi.h"
#include "board.h"
#include "tusb.h"
#include "cmsis_os.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define LINE_BUF_SIZE 128

/* ----------------------------------------------------------------
 * Error codes
 * ---------------------------------------------------------------- */
enum {
    ERR_ARGS    = 1,
    ERR_NOCARD  = 2,
    ERR_NOFILE  = 3,
    ERR_BUSY    = 4,
    ERR_IO      = 5,
    ERR_CRC     = 6,
    ERR_UNSUP   = 7,
};

/* ----------------------------------------------------------------
 * Static helpers
 * ---------------------------------------------------------------- */
static void reply_ok(const char *body) {
    char buf[LINE_BUF_SIZE];
    if (body && body[0]) {
        snprintf(buf, sizeof(buf), "OK %s\n", body);
    } else {
        snprintf(buf, sizeof(buf), "OK\n");
    }
    usb_cdc_send_str(buf);
}

static void reply_err(int code, const char *msg) {
    char buf[LINE_BUF_SIZE];
    snprintf(buf, sizeof(buf), "ERR %d %s\n", code, msg ? msg : "error");
    usb_cdc_send_str(buf);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) {
        *(--e) = '\0';
    }
    return s;
}

static int stoi(const char *s, int *out) {
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s) return -1;
    *out = (int)v;
    return 0;
}

/* ----------------------------------------------------------------
 * Command handlers
 * ---------------------------------------------------------------- */
static void cmd_idn(void) {
    char serial[32];
    board_usb_get_serial_str(serial, sizeof(serial));
    char buf[80];
    snprintf(buf, sizeof(buf), "OpenCode,STM32-Datalogger,1.0,%s", serial);
    reply_ok(buf);
}

static void cmd_time(const char *arg) {
    if (arg[0] == '\0') {
        Timestamp ts = rtc_get();
        char buf[32];
        snprintf(buf, sizeof(buf), "%lu.%03u", (unsigned long)ts.sec, (unsigned)ts.ms);
        reply_ok(buf);
    } else {
        /* Set RTC from epoch (integer seconds; ms ignored) */
        char *end;
        unsigned long epoch = strtoul(arg, &end, 10);
        if (end == arg || *end != '\0') {
            reply_err(ERR_ARGS, "time <epoch>");
            return;
        }
        rtc_set((uint32_t)epoch);
        reply_ok("");
    }
}

static void cmd_stat(void) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "logging=1 card=%d usb=%d heap=%u",
             sd_spi_card_present(),
             usb_mounted(),
             (unsigned)xPortGetFreeHeapSize());
    reply_ok(buf);
}

static int send_storage_cmd(StorageCmdId id, const char *arg) {
    StorageCmd c;
    c.cmd = id;
    strncpy(c.arg, arg ? arg : "", CMD_ARG_LEN - 1);
    c.arg[CMD_ARG_LEN - 1] = '\0';
    return (osMessageQueuePut(q_cmd, &c, 0, 1000) == osOK) ? 0 : -1;
}

static void cmd_log_start(void) {
    if (send_storage_cmd(SCMD_LOG_START, "") == 0) {
        /* response comes from storage task */
    } else {
        reply_err(ERR_BUSY, "cmd queue full");
    }
}

static void cmd_log_stop(void) {
    if (send_storage_cmd(SCMD_LOG_STOP, "") != 0) {
        reply_err(ERR_BUSY, "cmd queue full");
    }
}

static void cmd_rate(const char *arg) {
    int r;
    if (stoi(arg, &r) != 0 || r < 100 || r > 60000) {
        reply_err(ERR_ARGS, "rate ms");
        return;
    }
    /* Route through storage so the new rate is both applied and persisted
       to CONFIG.TXT; the OK/ERR reply is sent by the storage task. */
    char cfg[24];
    snprintf(cfg, sizeof(cfg), "rate=%d", r);
    if (send_storage_cmd(SCMD_CFG_SET, cfg) != 0) {
        reply_err(ERR_BUSY, "cmd queue full");
    }
}

static void cmd_thr(const char *arg) {
    int ch, lo, hi;
    if (sscanf(arg, "%d %d %d", &ch, &lo, &hi) != 3 || ch != 14 ||
        lo < 0 || lo > 4095 || hi < 0 || hi > 4095 || lo > hi) {
        reply_err(ERR_ARGS, "thr <ch> <lo> <hi>");
        return;
    }
    /* Thresholds are applied live (read by the sampler task); they are
       session-only and not persisted to CONFIG.TXT. */
    g_config.thr_lo = (uint16_t)lo;
    g_config.thr_hi = (uint16_t)hi;
    reply_ok("");
}

static void cmd_ls(const char *arg) {
    if (send_storage_cmd(SCMD_LS, arg) != 0) {
        reply_err(ERR_BUSY, "cmd queue full");
    }
}

static void cmd_get(const char *arg) {
    if (send_storage_cmd(SCMD_GET, arg) != 0) {
        reply_err(ERR_BUSY, "cmd queue full");
    }
}

static void cmd_del(const char *arg) {
    if (send_storage_cmd(SCMD_DEL, arg) != 0) {
        reply_err(ERR_BUSY, "cmd queue full");
    }
}

static void cmd_format(const char *arg) {
    if (send_storage_cmd(SCMD_FORMAT, arg) != 0) {
        reply_err(ERR_BUSY, "cmd queue full");
    }
}

static void cmd_stream(const char *arg) {
    if (strcmp(arg, "ON") == 0) {
        g_streaming = 1;
        reply_ok("");
    } else if (strcmp(arg, "OFF") == 0) {
        g_streaming = 0;
        reply_ok("");
    } else {
        reply_err(ERR_ARGS, "stream ON|OFF");
    }
}

static void cmd_cfg(const char *arg) {
    if (arg[0] == '\0' || strcmp(arg, "?") == 0) {
        if (send_storage_cmd(SCMD_CFG_GET, "") != 0) {
            reply_err(ERR_BUSY, "cmd queue full");
        }
    } else {
        if (send_storage_cmd(SCMD_CFG_SET, arg) != 0) {
            reply_err(ERR_BUSY, "cmd queue full");
        }
    }
}

/* ----------------------------------------------------------------
 * Dispatch
 * ---------------------------------------------------------------- */
static void dispatch(char *line) {
    line = trim(line);
    if (line[0] == '\0') return;

    /* Split command / argument */
    char *sp = strchr(line, ' ');
    char *arg = "";
    if (sp) {
        *sp = '\0';
        arg = trim(sp + 1);
    }

    if (strcmp(line, "*IDN?") == 0) {
        cmd_idn();
    } else if (strcmp(line, "TIME?") == 0) {
        cmd_time("");
    } else if (strcmp(line, "TIME") == 0) {
        cmd_time(arg);
    } else if (strcmp(line, "STAT?") == 0) {
        cmd_stat();
    } else if (strcmp(line, "LOG") == 0) {
        if (strcmp(arg, "START") == 0) cmd_log_start();
        else if (strcmp(arg, "STOP") == 0) cmd_log_stop();
        else reply_err(ERR_ARGS, "log start|stop");
    } else if (strcmp(line, "RATE") == 0) {
        cmd_rate(arg);
    } else if (strcmp(line, "THR") == 0) {
        cmd_thr(arg);
    } else if (strcmp(line, "LS") == 0) {
        cmd_ls(arg);
    } else if (strcmp(line, "GET") == 0) {
        cmd_get(arg);
    } else if (strcmp(line, "DEL") == 0) {
        cmd_del(arg);
    } else if (strcmp(line, "FORMAT") == 0) {
        cmd_format(arg);
    } else if (strcmp(line, "STREAM") == 0) {
        cmd_stream(arg);
    } else if (strcmp(line, "MSC") == 0) {
        if (strcmp(arg, "ON") == 0) {
            if (send_storage_cmd(SCMD_MSC_ON, "") != 0) reply_err(ERR_BUSY, "cmd queue full");
        } else if (strcmp(arg, "OFF") == 0) {
            if (send_storage_cmd(SCMD_MSC_OFF, "") != 0) reply_err(ERR_BUSY, "cmd queue full");
        } else {
            reply_err(ERR_ARGS, "msc ON|OFF");
        }
    } else if (strcmp(line, "CFG?") == 0 || strcmp(line, "CFG") == 0) {
        cmd_cfg(arg);
    } else {
        reply_err(ERR_ARGS, "unknown command");
    }
}

/* ----------------------------------------------------------------
 * Protocol task
 * ---------------------------------------------------------------- */
void task_proto_start(void *arg) {
    (void) arg;

    char line[LINE_BUF_SIZE];
    uint32_t pos = 0;

    for (;;) {
        if (tud_cdc_available()) {
            uint8_t ch;
            uint32_t n = tud_cdc_read(&ch, 1);
            if (n == 0) continue;

            if (ch == '\n') {
                line[pos] = '\0';
                dispatch(line);
                pos = 0;
            } else if (pos < sizeof(line) - 1) {
                line[pos++] = (char)ch;
            }
        } else {
            osDelay(1);
        }
    }
}
