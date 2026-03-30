#include <stdint.h>
#include <stddef.h>
#include "dtm_bindings.h"
#include "common/dtm_clk.h"
#include "common/dtm_delay.h"

/* Cortex-M SysTick 寄存器结构体定义 */
typedef struct {
    volatile uint32_t CSR;    /*!< SysTick Control and Status Register,        Address offset: 0x00 */
    volatile uint32_t RVR;    /*!< SysTick Reload Value Register,              Address offset: 0x04 */
    volatile uint32_t CVR;    /*!< SysTick Current Value Register,             Address offset: 0x08 */
    volatile uint32_t CALIB;  /*!< SysTick Calibration Register,               Address offset: 0x0C */
} SysTick_Type;

/* SysTick 基地址定义 */
#define SYSTICK_BASE   (0xE000E010U)
#define SYSTICK        ((SysTick_Type *)SYSTICK_BASE)

/* SysTick 控制和状态寄存器位定义 */
#define SYST_CSR_ENABLE_MASK  (0x00000001U)
#define SYST_CSR_TICKINT_MASK (0x00000002U)
#define SYST_CSR_CLKSOURCE_MASK (0x00000004U)
#define SYST_CSR_COUNTFLAG_MASK (0x00010000U)

/* 静态全局变量，用于存储系统时钟频率 */
static uint32_t sys_clock_freq = 0;
static dtm_delay_callback_t delay_callback = NULL;
static dtm_delay_mode_t delay_mode = DTM_DELAY_MODE_DELAY;

/* SysTick 初始化函数 */
void dtm_delay_init(void) {
    /* 获取系统时钟频率并存储 */
    sys_clock_freq = clk_get_rate(CLK_SYSCLK);
    /* 设置 SysTick 重载值为最大值 */
    SYSTICK->RVR = 0xFFFFFF;
    /* 启用 SysTick，使用核心时钟作为时钟源，不生成中断 */
    SYSTICK->CSR = SYST_CSR_ENABLE_MASK | SYST_CSR_CLKSOURCE_MASK;
}

/* 微秒延时函数 - Cortex-M 通用实现 */
void dtm_delay_us(uint32_t us) {
    uint32_t ticks = 0;
    uint32_t start = 0;
    uint32_t current = 0;

    /* 计算需要的时钟周期数，使用预存的系统时钟频率 */
    ticks = us * (sys_clock_freq / 1000000);
    
    /* 读取SysTick当前值 */
    start = SYSTICK->CVR;
    
    /* 循环等待直到达到指定的时钟周期数 */
    while (1) {
        current = SYSTICK->CVR;
        
        /* 处理SysTick计数器回绕 */
        if (start < current) {
            /* 回绕情况：计算已经过的时钟周期数 */
            current = start + (0xFFFFFF - current);
        } else {
            /* 正常情况：直接计算差值 */
            current = start - current;
        }
        
        /* 检查是否已经达到指定的时钟周期数 */
        if (current > ticks) {
            break;
        }
    }
}

/* 毫秒延时函数 - 基于微秒延时实现 */
void dtm_delay_ms(uint32_t ms) {
    while (ms--) {
        dtm_delay_us(1000);
    }
}

int dtm_delay_set_callback(dtm_delay_callback_t callback, uint32_t ticks, dtm_delay_mode_t mode) {
    if (ticks == 0 || ticks > 0xFFFFFF) {
        return -1;
    }
    
    delay_callback = callback;
    delay_mode = mode;
    
    SYSTICK->CSR = 0;
    SYSTICK->CVR = 0;
    SYSTICK->RVR = ticks - 1;
    SYSTICK->CSR = SYST_CSR_ENABLE_MASK | SYST_CSR_TICKINT_MASK | SYST_CSR_CLKSOURCE_MASK;
    
    return 0;
}

int dtm_delay_set_periodic(dtm_delay_callback_t callback, uint32_t hz) {
    if (hz == 0) {
        return -1;
    }
    
    uint32_t ticks = sys_clock_freq / hz;
    return dtm_delay_set_callback(callback, ticks, DTM_DELAY_MODE_PERIODIC);
}

int dtm_delay_set_oneshot(dtm_delay_callback_t callback, uint32_t us) {
    uint32_t ticks = us * (sys_clock_freq / 1000000);
    return dtm_delay_set_callback(callback, ticks, DTM_DELAY_MODE_ONESHOT);
}

void dtm_delay_stop(void) {
    SYSTICK->CSR = 0;
    delay_callback = NULL;
    delay_mode = DTM_DELAY_MODE_DELAY;
}

void SysTick_Handler(void) {
    
    if (delay_callback) {
        delay_callback();
    }
    
    if (delay_mode == DTM_DELAY_MODE_ONESHOT) {
        SYSTICK->CSR = 0;
        delay_callback = NULL;
        delay_mode = DTM_DELAY_MODE_DELAY;
    }
}
