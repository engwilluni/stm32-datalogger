#include "task_gpio.h"
#include "datalogger.h"
#include "rtc_time.h"
#include "records.h"
#include "main.h"
#include "cmsis_os.h"

/*
 * 6 monitored input pins, scanned every 5 ms.
 * Debounce: N=4 integrator → ~20 ms settling time.
 * One event emitted per confirmed state transition.
 */
#define NUM_INPUTS 6
#define DEBOUNCE_N 4

static GPIO_TypeDef *const s_ports[NUM_INPUTS] = {
    GPIOD, GPIOD, GPIOD, GPIOD, GPIOD, GPIOB
};
static const uint16_t s_pins[NUM_INPUTS] = {
    BtnSelect_Pin, BtnUp_Pin, BtnRight_Pin, BtnDown_Pin, BtnLeft_Pin, BtnUser_Pin
};

void task_gpio_start(void *arg) {
    (void)arg;
    uint8_t integrator[NUM_INPUTS] = {0};
    uint8_t state[NUM_INPUTS]      = {0};
    uint32_t wake = osKernelGetTickCount();

    for (;;) {
        osDelayUntil(wake += 5);  /* 5 ms polling period */

        for (int i = 0; i < NUM_INPUTS; i++) {
            uint8_t raw = (HAL_GPIO_ReadPin(s_ports[i], s_pins[i]) == GPIO_PIN_SET) ? 1u : 0u;

            if (raw) {
                if (integrator[i] < DEBOUNCE_N) integrator[i]++;
            } else {
                if (integrator[i] > 0)           integrator[i]--;
            }

            uint8_t debounced = state[i];
            if      (integrator[i] == DEBOUNCE_N) debounced = 1u;
            else if (integrator[i] == 0)           debounced = 0u;

            if (debounced != state[i]) {
                state[i] = debounced;

                LogRecord rec = {0};
                Timestamp ts  = rtc_get();
                rec.t_sec = ts.sec;
                rec.t_ms  = ts.ms;
                rec.type  = REC_TYPE_GPIO;
                rec.chan   = (uint8_t)i;
                rec.value  = debounced;
                rec.seq    = rec_next_seq();
                rec_seal(&rec);

                osMessageQueuePut(q_events, &rec, 0, 0);
            }
        }
    }
}
