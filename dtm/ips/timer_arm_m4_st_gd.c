#include "dtm_bindings.h"

#ifdef DTM_BINFDING_TIMER_ARM_M4_ST_GD
#include "dtm_timer.h"
#include "dtm_clk.h"
#include <stddef.h>

/* 定时器寄存器结构定义 */
typedef struct
{
  volatile uint32_t CR1;         /*!< 控制寄存器1,               Address offset: 0x00 */
  volatile uint32_t CR2;         /*!< 控制寄存器2,               Address offset: 0x04 */
  volatile uint32_t SMCR;        /*!< 从模式控制寄存器,           Address offset: 0x08 */
  volatile uint32_t DIER;        /*!< 中断使能寄存器,             Address offset: 0x0C */
  volatile uint32_t SR;          /*!< 状态寄存器,                 Address offset: 0x10 */
  volatile uint32_t EGR;         /*!< 事件生成寄存器,             Address offset: 0x14 */
  volatile uint32_t CCMR1;       /*!< 捕获/比较模式寄存器1,       Address offset: 0x18 */
  volatile uint32_t CCMR2;       /*!< 捕获/比较模式寄存器2,       Address offset: 0x1C */
  volatile uint32_t CCER;        /*!< 捕获/比较使能寄存器,        Address offset: 0x20 */
  volatile uint32_t CNT;         /*!< 计数器,                     Address offset: 0x24 */
  volatile uint32_t PSC;         /*!< 预分频器,                   Address offset: 0x28 */
  volatile uint32_t ARR;         /*!< 自动重载寄存器,             Address offset: 0x2C */
  volatile uint32_t RCR;         /*!< 重复计数器寄存器,           Address offset: 0x30 */
  volatile uint32_t CCR1;        /*!< 捕获/比较寄存器1,           Address offset: 0x34 */
  volatile uint32_t CCR2;        /*!< 捕获/比较寄存器2,           Address offset: 0x38 */
  volatile uint32_t CCR3;        /*!< 捕获/比较寄存器3,           Address offset: 0x3C */
  volatile uint32_t CCR4;        /*!< 捕获/比较寄存器4,           Address offset: 0x40 */
  volatile uint32_t BDTR;        /*!< 断路和死区时间寄存器,       Address offset: 0x44 */
  volatile uint32_t DCR;         /*!< DMA控制寄存器,              Address offset: 0x48 */
  volatile uint32_t DMAR;        /*!< DMA地址寄存器,              Address offset: 0x4C */
  volatile uint32_t OR;          /*!< 选项寄存器,                 Address offset: 0x50 */
} DTM_TIMER_Type;

/* 定时器回调函数数组 */
static dtm_timer_callback_t timer_callbacks[14] = {NULL};

/* 定时器基地址到索引的映射 */
static int timx_to_index(uint32_t timx)
{
    switch (timx) {
        case TIM1: return 0;
        case TIM2: return 1;
        case TIM3: return 2;
        case TIM4: return 3;
        case TIM5: return 4;
        case TIM6: return 5;
        case TIM7: return 6;
        case TIM8: return 7;
        case TIM9: return 8;
        case TIM10: return 9;
        case TIM11: return 10;
        case TIM12: return 11;
        case TIM13: return 12;
        case TIM14: return 13;
        default: return -1;
    }
}

/**
 * @brief       将定时器基地址转换为对应的时钟ID
 * @param       timx: 定时器基地址
 * @return      对应的时钟ID
 */
static int timer_to_clk_id(uint32_t timx)
{
    int clk_id = -1;
    
    switch (timx) {
        case TIM1: clk_id = CLK_TIM1; break;
        case TIM2: clk_id = CLK_TIM2; break;
        case TIM3: clk_id = CLK_TIM3; break;
        case TIM4: clk_id = CLK_TIM4; break;
        case TIM5: clk_id = CLK_TIM5; break;
        case TIM6: clk_id = CLK_TIM6; break;
        case TIM7: clk_id = CLK_TIM7; break;
        case TIM8: clk_id = CLK_TIM8; break;
        case TIM9: clk_id = CLK_TIM9; break;
        case TIM10: clk_id = CLK_TIM10; break;
        case TIM11: clk_id = CLK_TIM11; break;
        case TIM12: clk_id = CLK_TIM12; break;
        case TIM13: clk_id = CLK_TIM13; break;
        case TIM14: clk_id = CLK_TIM14; break;
        default: break;
    }
    
    return clk_id;
}

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
void dtm_timer_init(uint32_t timx, uint16_t psc, uint16_t arr, uint8_t mode)
{
    /* 使能定时器时钟 */
    int clk_id = timer_to_clk_id(timx);
    if (clk_id != -1) {
        clk_enable(clk_id);
    }
    
    DTM_TIMER_Type *p_timx = (DTM_TIMER_Type *)timx;
    
    /* 禁用定时器 */
    p_timx->CR1 &= ~(1 << 0);
    
    /* 设置计数模式 */
    p_timx->CR1 &= ~(3 << 4); /* 清除计数模式 */
    switch (mode) {
        case DTM_TIMER_MODE_UP:
            p_timx->CR1 &= ~(3 << 4);
            break;
        case DTM_TIMER_MODE_DOWN:
            p_timx->CR1 |= (1 << 4);
            break;
        case DTM_TIMER_MODE_CENTER_ALIGNED:
            p_timx->CR1 |= (2 << 4);
            break;
        default:
            break;
    }
    
    /* 设置预分频器和自动重载值 */
    p_timx->PSC = psc;
    p_timx->ARR = arr;
    
    /* 清除计数器 */
    p_timx->CNT = 0;
    
    /* 生成更新事件，加载寄存器值 */
    p_timx->EGR |= (1 << 0);
}

/**
 * @brief       使能定时器
 * @param       timx: 定时器基地址
 * @retval      None
 */
void dtm_timer_enable(uint32_t timx)
{
    DTM_TIMER_Type *p_timx = (DTM_TIMER_Type *)timx;
    p_timx->CR1 |= (1 << 0);
}

/**
 * @brief       禁用定时器
 * @param       timx: 定时器基地址
 * @retval      None
 */
void dtm_timer_disable(uint32_t timx)
{
    DTM_TIMER_Type *p_timx = (DTM_TIMER_Type *)timx;
    p_timx->CR1 &= ~(1 << 0);
}

/**
 * @brief       设置定时器预分频器
 * @param       timx: 定时器基地址
 * @param       psc: 预分频器值 (0-65535)
 * @retval      None
 */
void dtm_timer_set_psc(uint32_t timx, uint16_t psc)
{
    DTM_TIMER_Type *p_timx = (DTM_TIMER_Type *)timx;
    p_timx->PSC = psc;
    /* 生成更新事件，加载寄存器值 */
    p_timx->EGR |= (1 << 0);
}

/**
 * @brief       设置定时器自动重载值
 * @param       timx: 定时器基地址
 * @param       arr: 自动重载值 (0-65535)
 * @retval      None
 */
void dtm_timer_set_arr(uint32_t timx, uint16_t arr)
{
    DTM_TIMER_Type *p_timx = (DTM_TIMER_Type *)timx;
    p_timx->ARR = arr;
    /* 生成更新事件，加载寄存器值 */
    p_timx->EGR |= (1 << 0);
}

/**
 * @brief       获取定时器当前计数值
 * @param       timx: 定时器基地址
 * @retval      当前计数值
 */
uint16_t dtm_timer_get_cnt(uint32_t timx)
{
    DTM_TIMER_Type *p_timx = (DTM_TIMER_Type *)timx;
    return (uint16_t)p_timx->CNT;
}

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
void dtm_timer_clear_flag(uint32_t timx, uint32_t flag)
{
    DTM_TIMER_Type *p_timx = (DTM_TIMER_Type *)timx;
    p_timx->SR &= ~flag;
}

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
void dtm_timer_enable_it(uint32_t timx, uint32_t it)
{
    DTM_TIMER_Type *p_timx = (DTM_TIMER_Type *)timx;
    p_timx->DIER |= it;
}

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
void dtm_timer_disable_it(uint32_t timx, uint32_t it)
{
    DTM_TIMER_Type *p_timx = (DTM_TIMER_Type *)timx;
    p_timx->DIER &= ~it;
}

/**
 * @brief       设置定时器中断回调函数
 * @param       timx: 定时器基地址
 * @param       callback: 回调函数指针
 * @retval      None
 */
void dtm_timer_set_callback(uint32_t timx, dtm_timer_callback_t callback)
{
    int index = timx_to_index(timx);
    if (index >= 0 && index < 14) {
        timer_callbacks[index] = callback;
    }
}

/**
 * @brief       定时器中断处理函数
 * @param       timx: 定时器基地址
 * @retval      None
 */
void dtm_timer_irq_handler(uint32_t timx)
{
    DTM_TIMER_Type *p_timx = (DTM_TIMER_Type *)timx;

    /* 清除中断标志 */
    p_timx->SR = 0;
    
    /* 调用回调函数 */
    int index = timx_to_index(timx);
    if (index >= 0 && index < 14 && timer_callbacks[index]) {
        timer_callbacks[index](timx);
    }
}

/* 定时器中断处理函数 */
void TIM1_BRK_TIM9_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM1);
    dtm_timer_irq_handler(TIM9);
}

void TIM1_UP_TIM10_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM1);
    dtm_timer_irq_handler(TIM10);
}

void TIM1_TRG_COM_TIM11_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM1);
    dtm_timer_irq_handler(TIM11);
}

void TIM1_CC_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM1);
}

void TIM2_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM2);
}

void TIM3_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM3);
}

void TIM4_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM4);
}

void TIM8_BRK_TIM12_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM8);
    dtm_timer_irq_handler(TIM12);
}

void TIM8_UP_TIM13_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM8);
    dtm_timer_irq_handler(TIM13);
}

void TIM8_TRG_COM_TIM14_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM8);
    dtm_timer_irq_handler(TIM14);
}

void TIM8_CC_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM8);
}

void TIM5_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM5);
}

void TIM6_DAC_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM6);
}

void TIM7_IRQHandler(void)
{
    dtm_timer_irq_handler(TIM7);
}

#endif /* DTM_BINFDING_TIMER_ARM_M4_ST_GD */
