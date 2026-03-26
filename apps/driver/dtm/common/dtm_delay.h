#ifndef __DTM_DELAY_H__
#define __DTM_DELAY_H__

#include <stdint.h>

typedef void (*dtm_delay_callback_t)(void);

typedef enum {
    DTM_DELAY_MODE_DELAY = 0,
    DTM_DELAY_MODE_ONESHOT,
    DTM_DELAY_MODE_PERIODIC
} dtm_delay_mode_t;

void dtm_delay_init(void);

/* 微秒延时函数 - Cortex-M 通用实现 */
void dtm_delay_us(uint32_t us);

/* 毫秒延时函数 - 基于微秒延时实现 */
void dtm_delay_ms(uint32_t ms);

int dtm_delay_set_callback(dtm_delay_callback_t callback, uint32_t ticks, dtm_delay_mode_t mode);
int dtm_delay_set_periodic(dtm_delay_callback_t callback, uint32_t hz);
int dtm_delay_set_oneshot(dtm_delay_callback_t callback, uint32_t us);
void dtm_delay_stop(void);

#endif
