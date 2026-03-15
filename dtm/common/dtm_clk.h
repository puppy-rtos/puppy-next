

/* 通用时钟树节点描述：节点id，父节点， */

/*  */


#pragma once

#include <stdint.h>

/* 时钟ID枚举 */
enum clk_id {
    CLK_HSI,
    CLK_HSE,
    CLK_PLL,
    CLK_PLL_VCO,
    CLK_PLL_P,
    CLK_PLL_Q,
    CLK_SYSCLK,
    CLK_HCLK,
    CLK_PCLK1,
    CLK_PCLK2,
    CLK_PCLK3,
    CLK_PCLK4,
    /* GPIO 时钟 */
    CLK_GPIOA,
    CLK_GPIOB,
    CLK_GPIOC,
    CLK_GPIOD,
    CLK_GPIOE,
    CLK_GPIOF,
    CLK_GPIOG,
    CLK_GPIOH,
    CLK_GPIOI,
    /* UART 时钟 */
    CLK_UART1,
    CLK_UART2,
    CLK_UART3,
    CLK_UART4,
    CLK_UART5,
    CLK_UART6,
    CLK_UART7,
    CLK_UART8,
    /* Timer 时钟 */
    CLK_TIM1,
    CLK_TIM2,
    CLK_TIM3,
    CLK_TIM4,
    CLK_TIM5,
    CLK_TIM6,
    CLK_TIM7,
    CLK_TIM8,
    CLK_TIM9,
    CLK_TIM10,
    CLK_TIM11,
    CLK_TIM12,
    CLK_TIM13,
    CLK_TIM14,
    /* SPI 时钟 */
    CLK_SPI1,
    CLK_SPI2,
    CLK_SPI3,
    CLK_COUNT /* 时钟总数 */
};

/* 时钟节点描述结构体 */
struct clk_desc {
    const char *name;       /* 时钟名称 */
    uint32_t freq;          /* 静态频率（如果有） */
    int parent;             /* 父时钟ID，-1表示无父节点 */
    const struct clk_ops *ops; /* 时钟操作函数 */
    uint32_t div;           /* 静态分频系数（如果有） */
    /* PLL 特定参数 */
    uint32_t plln;          /* PLL 倍频系数 */
    uint32_t pllm;          /* PLL 预分频系数 */
    uint32_t pllp;          /* PLL P分频系数 */
    uint32_t pllq;          /* PLL Q分频系数 */
    uint32_t pllr;          /* PLL R分频系数 */
    /* 外设时钟特定参数 */
    volatile uint32_t *enr_reg;   /* 时钟使能寄存器地址 */
    uint32_t enr_bit;             /* 时钟使能位掩码 */
    /* 运行时状态 */
    uint32_t rate;          /* 当前时钟频率 */
    int enabled;            /* 时钟使能状态 */
};

/* 时钟操作函数结构体 */
struct clk_ops {
    uint32_t (*get_rate)(const struct clk_desc *clk);
    int (*enable)(struct clk_desc *clk);
    int (*disable)(struct clk_desc *clk);
    int (*set_parent)(struct clk_desc *clk, int parent_id);
    int (*set_rate)(struct clk_desc *clk, uint32_t rate);
};

/* 外部声明时钟树数组 */
extern struct clk_desc dtm_clks[CLK_COUNT];

/* 时钟相关函数声明 */
void dtm_clk_init(void);
void dtm_clk_dumptree(void);
uint8_t sys_clock_set(void);
void sys_stm32_clock_init(void);

/* 公共时钟API函数声明 */
uint32_t clk_get_rate(int clk_id);
int clk_enable(int clk_id);
int clk_disable(int clk_id);
int clk_set_parent(int clk_id, int parent_id);
int clk_set_rate(int clk_id, uint32_t rate);