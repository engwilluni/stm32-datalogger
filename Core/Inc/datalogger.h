#ifndef DATALOGGER_H
#define DATALOGGER_H

#include "cmsis_os.h"
#include "records.h"

/* ---- Shared IPC ---- */
extern osMessageQueueId_t q_events;   /* LogRecord queue: sampler + gpio → storage */

/* ---- Command channel: proto → storage ---- */
#define CMD_ARG_LEN 64

typedef enum {
    SCMD_NONE = 0,
    SCMD_LS,
    SCMD_GET,
    SCMD_DEL,
    SCMD_FORMAT,
    SCMD_CFG_GET,
    SCMD_CFG_SET,
    SCMD_LOG_START,
    SCMD_LOG_STOP,
    SCMD_MSC_ON,
    SCMD_MSC_OFF,
} StorageCmdId;

typedef struct {
    StorageCmdId cmd;
    char         arg[CMD_ARG_LEN];
} StorageCmd;

extern osMessageQueueId_t q_cmd;      /* proto → storage file commands */

/* ---- Runtime configuration (loaded from CONFIG.TXT by storage task) ---- */
typedef struct {
    uint32_t sample_rate_ms;  /* ADC sample interval in ms (default 1000) */
    uint8_t  log_format;      /* 0 = binary, 1 = CSV */
    uint16_t thr_lo;          /* ADC alarm lower bound (0 = disabled)    */
    uint16_t thr_hi;          /* ADC alarm upper bound (4095 = disabled)  */
    uint8_t  oversample;      /* ADC samples to average (1–16)            */
} AppConfig;

extern AppConfig g_config;

/* ---- Streaming flag (set by proto, read by storage) ---- */
extern uint8_t g_streaming;

/* ---- MSC mass-storage mode flag ---- */
extern uint8_t g_msc_enabled;

/* ---- Recursive mutex protecting SD-SPI access (storage <> MSC) ---- */
extern osMutexId_t mtx_sd;

#endif /* DATALOGGER_H */
