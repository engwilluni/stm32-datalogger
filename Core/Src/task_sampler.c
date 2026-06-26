#include "task_sampler.h"
#include "datalogger.h"
#include "rtc_time.h"
#include "records.h"
#include "main.h"
#include "cmsis_os.h"

extern ADC_HandleTypeDef hadc1;

void task_sampler_start(void *arg) {
    (void)arg;
    uint32_t wake = osKernelGetTickCount();

    for (;;) {
        osDelayUntil(wake += g_config.sample_rate_ms);

        /* Take N samples and average (oversampling) */
        uint32_t acc = 0;
        uint8_t  n   = g_config.oversample;
        for (uint8_t i = 0; i < n; i++) {
            HAL_ADC_Start(&hadc1);
            if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
                acc += HAL_ADC_GetValue(&hadc1);
            }
            HAL_ADC_Stop(&hadc1);
        }
        uint16_t raw = (uint16_t)(acc / n);

        LogRecord rec = {0};
        Timestamp ts  = rtc_get();
        rec.t_sec = ts.sec;
        rec.t_ms  = ts.ms;
        rec.type  = REC_TYPE_ADC;
        rec.chan   = 14;           /* ADC1 channel 14 = PC4 potentiometer */
        rec.value  = raw;
        rec.seq    = rec_next_seq();
        rec_seal(&rec);

        osMessageQueuePut(q_events, &rec, 0, 0);  /* drop if queue full */

        /* Threshold alarm: emit MARKER when crossing out of [thr_lo, thr_hi] */
        static uint8_t s_alarm = 0;
        uint8_t out = (raw < g_config.thr_lo || raw > g_config.thr_hi) ? 1u : 0u;
        if (out && !s_alarm) {
            s_alarm = 1;
            LogRecord alarm = {0};
            alarm.t_sec = ts.sec;
            alarm.t_ms  = ts.ms;
            alarm.type  = REC_TYPE_MARKER;
            alarm.chan  = 14;
            alarm.value = raw;
            alarm.seq   = rec_next_seq();
            rec_seal(&alarm);
            osMessageQueuePut(q_events, &alarm, 0, 0);
        } else if (!out && s_alarm) {
            s_alarm = 0;
        }
    }
}
