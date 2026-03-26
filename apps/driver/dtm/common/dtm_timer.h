#include <stdint.h>

/* 定时器模式定义 */
#define DTM_TIMER_MODE_UP           0       /* 向上计数模式 */
#define DTM_TIMER_MODE_DOWN         1       /* 向下计数模式 */
#define DTM_TIMER_MODE_CENTER_ALIGNED 2    /* 中心对齐模式 */

/* 定时器预分频器和自动重载值最大值 */
#define DTM_TIMER_PSC_MAX           0xFFFF  /* 16位预分频器最大值 */
#define DTM_TIMER_ARR_MAX           0xFFFF  /* 16位自动重载值最大值 */

/* 定时器中断标志位 */
#define DTM_TIMER_FLAG_UPDATE       (1 << 0)    /* 更新中断标志 */
#define DTM_TIMER_FLAG_CC1          (1 << 1)    /* 通道1捕获/比较标志 */
#define DTM_TIMER_FLAG_CC2          (1 << 2)    /* 通道2捕获/比较标志 */
#define DTM_TIMER_FLAG_CC3          (1 << 3)    /* 通道3捕获/比较标志 */
#define DTM_TIMER_FLAG_CC4          (1 << 4)    /* 通道4捕获/比较标志 */
#define DTM_TIMER_FLAG_TRIGGER      (1 << 6)    /* 触发事件标志 */
#define DTM_TIMER_FLAG_COM          (1 << 7)    /* 互补输出标志 */
#define DTM_TIMER_FLAG_BREAK        (1 << 8)    /* 断路标志 */

/* 定时器中断使能位 */
#define DTM_TIMER_IT_UPDATE         (1 << 0)    /* 更新中断使能 */
#define DTM_TIMER_IT_CC1            (1 << 1)    /* 通道1捕获/比较中断使能 */
#define DTM_TIMER_IT_CC2            (1 << 2)    /* 通道2捕获/比较中断使能 */
#define DTM_TIMER_IT_CC3            (1 << 3)    /* 通道3捕获/比较中断使能 */
#define DTM_TIMER_IT_CC4            (1 << 4)    /* 通道4捕获/比较中断使能 */
#define DTM_TIMER_IT_TRIGGER        (1 << 6)    /* 触发事件中断使能 */
#define DTM_TIMER_IT_COM            (1 << 7)    /* 互补输出中断使能 */
#define DTM_TIMER_IT_BREAK          (1 << 8)    /* 断路中断使能 */

/* 定时器回调函数类型 */
typedef void (*dtm_timer_callback_t)(uint32_t timx);

/**
 * @brief       定时器初始化
 * @param       timx: 定时器基地址
 * @param       psc: 预分频器值 (0-65535)
 * @param       arr: 自动重载值 (0-65535)
 * @param       mode: 计数模式
 *   @arg       DTM_TIMER_MODE_UP: 向上计数模式
 *   @arg       DTM_TIMER_MODE_DOWN: 向下计数模式
 *   @arg       DTM_TIMER_MODE_CENTER_ALIGNED: 中心对齐模式
 * @retval      None
 */
void dtm_timer_init(uint32_t timx, uint16_t psc, uint16_t arr, uint8_t mode);

/**
 * @brief       使能定时器
 * @param       timx: 定时器基地址
 * @retval      None
 */
void dtm_timer_enable(uint32_t timx);

/**
 * @brief       禁用定时器
 * @param       timx: 定时器基地址
 * @retval      None
 */
void dtm_timer_disable(uint32_t timx);

/**
 * @brief       设置定时器预分频器
 * @param       timx: 定时器基地址
 * @param       psc: 预分频器值 (0-65535)
 * @retval      None
 */
void dtm_timer_set_psc(uint32_t timx, uint16_t psc);

/**
 * @brief       设置定时器自动重载值
 * @param       timx: 定时器基地址
 * @param       arr: 自动重载值 (0-65535)
 * @retval      None
 */
void dtm_timer_set_arr(uint32_t timx, uint16_t arr);

/**
 * @brief       获取定时器当前计数值
 * @param       timx: 定时器基地址
 * @retval      当前计数值
 */
uint16_t dtm_timer_get_cnt(uint32_t timx);

/**
 * @brief       清除定时器中断标志
 * @param       timx: 定时器基地址
 * @param       flag: 中断标志
 *   @arg       DTM_TIMER_FLAG_UPDATE: 更新中断标志
 *   @arg       DTM_TIMER_FLAG_CC1: 通道1捕获/比较标志
 *   @arg       DTM_TIMER_FLAG_CC2: 通道2捕获/比较标志
 *   @arg       DTM_TIMER_FLAG_CC3: 通道3捕获/比较标志
 *   @arg       DTM_TIMER_FLAG_CC4: 通道4捕获/比较标志
 *   @arg       DTM_TIMER_FLAG_TRIGGER: 触发事件标志
 *   @arg       DTM_TIMER_FLAG_COM: 互补输出标志
 *   @arg       DTM_TIMER_FLAG_BREAK: 断路标志
 * @retval      None
 */
void dtm_timer_clear_flag(uint32_t timx, uint32_t flag);

/**
 * @brief       使能定时器中断
 * @param       timx: 定时器基地址
 * @param       it: 中断类型
 *   @arg       DTM_TIMER_IT_UPDATE: 更新中断
 *   @arg       DTM_TIMER_IT_CC1: 通道1捕获/比较中断
 *   @arg       DTM_TIMER_IT_CC2: 通道2捕获/比较中断
 *   @arg       DTM_TIMER_IT_CC3: 通道3捕获/比较中断
 *   @arg       DTM_TIMER_IT_CC4: 通道4捕获/比较中断
 *   @arg       DTM_TIMER_IT_TRIGGER: 触发事件中断
 *   @arg       DTM_TIMER_IT_COM: 互补输出中断
 *   @arg       DTM_TIMER_IT_BREAK: 断路中断
 * @retval      None
 */
void dtm_timer_enable_it(uint32_t timx, uint32_t it);

/**
 * @brief       禁用定时器中断
 * @param       timx: 定时器基地址
 * @param       it: 中断类型
 *   @arg       DTM_TIMER_IT_UPDATE: 更新中断
 *   @arg       DTM_TIMER_IT_CC1: 通道1捕获/比较中断
 *   @arg       DTM_TIMER_IT_CC2: 通道2捕获/比较中断
 *   @arg       DTM_TIMER_IT_CC3: 通道3捕获/比较中断
 *   @arg       DTM_TIMER_IT_CC4: 通道4捕获/比较中断
 *   @arg       DTM_TIMER_IT_TRIGGER: 触发事件中断
 *   @arg       DTM_TIMER_IT_COM: 互补输出中断
 *   @arg       DTM_TIMER_IT_BREAK: 断路中断
 * @retval      None
 */
void dtm_timer_disable_it(uint32_t timx, uint32_t it);

/**
 * @brief       设置定时器中断回调函数
 * @param       timx: 定时器基地址
 * @param       callback: 回调函数指针
 * @retval      None
 */
void dtm_timer_set_callback(uint32_t timx, dtm_timer_callback_t callback);
