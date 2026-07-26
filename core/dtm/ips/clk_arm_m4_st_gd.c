
#include "dtm_bindings.h"

#ifdef DTM_BINFDING_CLK_ARM_M4_ST_GD

#include <stdint.h>
#include "../common/dtm_clk.h"
#include "../common/dtm_uart.h"
#include "../common/dtm_board.h"
#include "../common/dtm_of_driver.h"

/* RCC寄存器结构定义 */
typedef struct
{
  volatile uint32_t CR;            /*!< RCC clock control register,                                  Address offset: 0x00 */
  volatile uint32_t PLLCFGR;       /*!< RCC PLL configuration register,                              Address offset: 0x04 */
  volatile uint32_t CFGR;          /*!< RCC clock configuration register,                            Address offset: 0x08 */
  volatile uint32_t CIR;           /*!< RCC clock interrupt register,                                Address offset: 0x0C */
  volatile uint32_t AHB1RSTR;      /*!< RCC AHB1 peripheral reset register,                          Address offset: 0x10 */
  volatile uint32_t AHB2RSTR;      /*!< RCC AHB2 peripheral reset register,                          Address offset: 0x14 */
  volatile uint32_t AHB3RSTR;      /*!< RCC AHB3 peripheral reset register,                          Address offset: 0x18 */
  uint32_t      RESERVED0;     /*!< Reserved, 0x1C                                                                    */
  volatile uint32_t APB1RSTR;      /*!< RCC APB1 peripheral reset register,                          Address offset: 0x20 */
  volatile uint32_t APB2RSTR;      /*!< RCC APB2 peripheral reset register,                          Address offset: 0x24 */
  uint32_t      RESERVED1[2];  /*!< Reserved, 0x28-0x2C                                                               */
  volatile uint32_t AHB1ENR;       /*!< RCC AHB1 peripheral clock register,                          Address offset: 0x30 */
  volatile uint32_t AHB2ENR;       /*!< RCC AHB2 peripheral clock register,                          Address offset: 0x34 */
  volatile uint32_t AHB3ENR;       /*!< RCC AHB3 peripheral clock register,                          Address offset: 0x38 */
  uint32_t      RESERVED2;     /*!< Reserved, 0x3C                                                                    */
  volatile uint32_t APB1ENR;       /*!< RCC APB1 peripheral clock enable register,                   Address offset: 0x40 */
  volatile uint32_t APB2ENR;       /*!< RCC APB2 peripheral clock enable register,                   Address offset: 0x44 */
  uint32_t      RESERVED3[2];  /*!< Reserved, 0x48-0x4C                                                               */
  volatile uint32_t AHB1LPENR;     /*!< RCC AHB1 peripheral clock enable in low power mode register, Address offset: 0x50 */
  volatile uint32_t AHB2LPENR;     /*!< RCC AHB2 peripheral clock enable in low power mode register, Address offset: 0x54 */
  volatile uint32_t AHB3LPENR;     /*!< RCC AHB3 peripheral clock enable in low power mode register, Address offset: 0x58 */
  uint32_t      RESERVED4;     /*!< Reserved, 0x5C                                                                    */
  volatile uint32_t APB1LPENR;     /*!< RCC APB1 peripheral clock enable in low power mode register, Address offset: 0x60 */
  volatile uint32_t APB2LPENR;     /*!< RCC APB2 peripheral clock enable in low power mode register, Address offset: 0x64 */
  uint32_t      RESERVED5[2];  /*!< Reserved, 0x68-0x6C                                                               */
  volatile uint32_t BDCR;          /*!< RCC Backup domain control register,                          Address offset: 0x70 */
  volatile uint32_t CSR;           /*!< RCC clock control & status register,                         Address offset: 0x74 */
  uint32_t      RESERVED6[2];  /*!< Reserved, 0x78-0x7C                                                               */
  volatile uint32_t SSCGR;         /*!< RCC spread spectrum clock generation register,               Address offset: 0x80 */
  volatile uint32_t PLLI2SCFGR;    /*!< RCC PLLI2S configuration register,                           Address offset: 0x84 */
} DTM_RCC_Type;


#ifndef DTM_CLK_BASE
#define DTM_CLK_BASE 0x40023800UL
#endif

static uint32_t g_rcc_base = DTM_CLK_BASE;

#define RCC ((DTM_RCC_Type *)g_rcc_base)
#define RCC_INIT_REG(member) (&((DTM_RCC_Type *)DTM_CLK_BASE)->member)
/* 寄存器位定义 */
#define RCC_CR_HSION      (1 << 0)
#define RCC_CR_HSEON      (1 << 16)
#define RCC_CR_HSERDY     (1 << 17)
#define RCC_CR_PLLON      (1 << 24)
#define RCC_CR_PLLRDY     (1 << 25)

#define RCC_CFGR_SW       (3 << 0)  // 系统时钟选择
#define RCC_CFGR_SW_HSI   (0 << 0)
#define RCC_CFGR_SW_HSE   (1 << 0)
#define RCC_CFGR_SW_PLL   (2 << 0)

#define RCC_CFGR_SWS      (3 << 2)  // 系统时钟状态
#define RCC_CFGR_SWS_HSI  (0 << 2)
#define RCC_CFGR_SWS_HSE  (1 << 2)
#define RCC_CFGR_SWS_PLL  (2 << 2)

/* 固定频率时钟操作函数 */
static uint32_t fixed_clk_get_rate(const struct clk_desc *clk)
{
    return clk->freq;
}

static int fixed_clk_enable(struct clk_desc *clk)
{
    if (clk == &dtm_clks[CLK_HSI]) {
        RCC->CR |= RCC_CR_HSION;
        while (!(RCC->CR & (1 << 1))); /* 等待HSI就绪 */
    } else if (clk == &dtm_clks[CLK_HSE]) {
        RCC->CR |= RCC_CR_HSEON;
        while (!(RCC->CR & RCC_CR_HSERDY)); /* 等待HSE就绪 */
    }
    clk->enabled = 1;
    return 0;
}

static int fixed_clk_disable(struct clk_desc *clk)
{
    if (clk == &dtm_clks[CLK_HSI]) {
        RCC->CR &= ~RCC_CR_HSION;
    } else if (clk == &dtm_clks[CLK_HSE]) {
        RCC->CR &= ~RCC_CR_HSEON;
    }
    clk->enabled = 0;
    return 0;
}

static int fixed_clk_set_parent(struct clk_desc *clk, int parent_id)
{
    return -1; /* 固定频率时钟没有父节点 */
}

static int fixed_clk_set_rate(struct clk_desc *clk, uint32_t rate)
{
    return -1; /* 固定频率时钟不能设置频率 */
}

static const struct clk_ops fixed_clk_ops = {
    .get_rate = fixed_clk_get_rate,
    .enable = fixed_clk_enable,
    .disable = fixed_clk_disable,
    .set_parent = fixed_clk_set_parent,
    .set_rate = fixed_clk_set_rate,
};

static void dtm_clk_apply_gate(uint32_t clk_id,
                               uint32_t parent_id,
                               uint32_t enable_offset,
                               uint32_t enable_bit)
{
    if (clk_id >= CLK_COUNT || enable_bit >= 32U) {
        return;
    }

    if (parent_id < CLK_COUNT) {
        dtm_clks[clk_id].parent = (int)parent_id;
    }
    dtm_clks[clk_id].enr_reg = (volatile uint32_t *)(uintptr_t)(g_rcc_base + enable_offset);
    dtm_clks[clk_id].enr_bit = 1UL << enable_bit;
}

static void dtm_clk_parse_gate_nodes(dtm_ofw_node_t controller)
{
    dtm_ofw_node_t child = DTM_OFW_NODE_INVALID;

    while ((child = dtm_ofw_get_next_child(controller, child)) >= 0) {
        uint32_t clk_id;
        uint32_t parent_id;
        uint32_t enable_offset;
        uint32_t enable_bit;

        if (!dtm_ofw_node_is_available(child) ||
            !dtm_ofw_node_is_compatible(child, "puppy,clock-gate")) {
            continue;
        }

        if (dtm_ofw_prop_read_u32(child, "puppy,clock-id", &clk_id) < 0 ||
            dtm_ofw_prop_read_u32(child, "puppy,enable-reg", &enable_offset) < 0 ||
            dtm_ofw_prop_read_u32(child, "puppy,enable-bit", &enable_bit) < 0) {
            continue;
        }

        if (dtm_ofw_prop_read_u32(child, "puppy,parent-clock", &parent_id) < 0) {
            parent_id = CLK_COUNT;
        }

        dtm_clk_apply_gate(clk_id, parent_id, enable_offset, enable_bit);
    }
}

static void dtm_clk_apply_default_register_refs(void);

static int dtm_clk_arm_m4_st_gd_of_probe(dtm_ofw_node_t node)
{
    pup_dtm_clock_config_t config;
    uint64_t addr = 0;
    uint32_t value;

    if (dtm_board_get_clock(&config) != 0) {
        return -1;
    }

    if (dtm_ofw_get_reg(node, 0, &addr, 0) == 0) {
        config.valid = 1U;
        config.base = (uint32_t)addr;
        g_rcc_base = config.base;
    }

    if (dtm_ofw_prop_read_u32(node, "hse-frequency", &value) == 0) {
        config.hse_frequency = value;
    }
    if (dtm_ofw_prop_read_u32(node, "clock-frequency", &value) == 0) {
        config.sysclk_frequency = value;
    }
    if (dtm_ofw_prop_read_u32_index(node, "puppy,pll", 0, &value) == 0) {
        config.pll_m = value;
    }
    if (dtm_ofw_prop_read_u32_index(node, "puppy,pll", 1, &value) == 0) {
        config.pll_n = value;
    }
    if (dtm_ofw_prop_read_u32_index(node, "puppy,pll", 2, &value) == 0) {
        config.pll_p = value;
    }
    if (dtm_ofw_prop_read_u32_index(node, "puppy,pll", 3, &value) == 0) {
        config.pll_q = value;
    }
    if (dtm_ofw_prop_read_u32_index(node, "puppy,bus-dividers", 0, &value) == 0) {
        config.hclk_divider = value;
    }
    if (dtm_ofw_prop_read_u32_index(node, "puppy,bus-dividers", 1, &value) == 0) {
        config.pclk1_divider = value;
    }
    if (dtm_ofw_prop_read_u32_index(node, "puppy,bus-dividers", 2, &value) == 0) {
        config.pclk2_divider = value;
    }

    dtm_clk_parse_gate_nodes(node);
    dtm_clk_apply_default_register_refs();
    dtm_board_set_clock(&config);
    return 0;
}

static void dtm_clk_apply_default_register_refs(void)
{
    /* Bind every peripheral clock to its RCC enable register. Any entry that
     * already has a DTS-driven override keeps that value, so DTS still wins. */
    if (g_rcc_base == 0U) {
        return;
    }
    if (dtm_clks[CLK_GPIOA].enr_reg == NULL) dtm_clks[CLK_GPIOA].enr_reg = &RCC->AHB1ENR;
    if (dtm_clks[CLK_GPIOB].enr_reg == NULL) dtm_clks[CLK_GPIOB].enr_reg = &RCC->AHB1ENR;
    if (dtm_clks[CLK_GPIOC].enr_reg == NULL) dtm_clks[CLK_GPIOC].enr_reg = &RCC->AHB1ENR;
    if (dtm_clks[CLK_GPIOD].enr_reg == NULL) dtm_clks[CLK_GPIOD].enr_reg = &RCC->AHB1ENR;
    if (dtm_clks[CLK_GPIOE].enr_reg == NULL) dtm_clks[CLK_GPIOE].enr_reg = &RCC->AHB1ENR;
    if (dtm_clks[CLK_GPIOF].enr_reg == NULL) dtm_clks[CLK_GPIOF].enr_reg = &RCC->AHB1ENR;
    if (dtm_clks[CLK_GPIOG].enr_reg == NULL) dtm_clks[CLK_GPIOG].enr_reg = &RCC->AHB1ENR;
    if (dtm_clks[CLK_GPIOH].enr_reg == NULL) dtm_clks[CLK_GPIOH].enr_reg = &RCC->AHB1ENR;
    if (dtm_clks[CLK_GPIOI].enr_reg == NULL) dtm_clks[CLK_GPIOI].enr_reg = &RCC->AHB1ENR;

    if (dtm_clks[CLK_UART1].enr_reg == NULL) dtm_clks[CLK_UART1].enr_reg = &RCC->APB2ENR;
    if (dtm_clks[CLK_UART2].enr_reg == NULL) dtm_clks[CLK_UART2].enr_reg = &RCC->APB1ENR;
    if (dtm_clks[CLK_UART3].enr_reg == NULL) dtm_clks[CLK_UART3].enr_reg = &RCC->APB1ENR;
    if (dtm_clks[CLK_UART4].enr_reg == NULL) dtm_clks[CLK_UART4].enr_reg = &RCC->APB1ENR;
    if (dtm_clks[CLK_UART5].enr_reg == NULL) dtm_clks[CLK_UART5].enr_reg = &RCC->APB1ENR;
    if (dtm_clks[CLK_UART6].enr_reg == NULL) dtm_clks[CLK_UART6].enr_reg = &RCC->APB2ENR;
    if (dtm_clks[CLK_UART7].enr_reg == NULL) dtm_clks[CLK_UART7].enr_reg = &RCC->APB1ENR;
    if (dtm_clks[CLK_UART8].enr_reg == NULL) dtm_clks[CLK_UART8].enr_reg = &RCC->APB1ENR;

    if (dtm_clks[CLK_TIM1].enr_reg == NULL)  dtm_clks[CLK_TIM1].enr_reg  = &RCC->APB2ENR;
    if (dtm_clks[CLK_TIM2].enr_reg == NULL)  dtm_clks[CLK_TIM2].enr_reg  = &RCC->APB1ENR;
    if (dtm_clks[CLK_TIM3].enr_reg == NULL)  dtm_clks[CLK_TIM3].enr_reg  = &RCC->APB1ENR;
    if (dtm_clks[CLK_TIM4].enr_reg == NULL)  dtm_clks[CLK_TIM4].enr_reg  = &RCC->APB1ENR;
    if (dtm_clks[CLK_TIM5].enr_reg == NULL)  dtm_clks[CLK_TIM5].enr_reg  = &RCC->APB1ENR;
    if (dtm_clks[CLK_TIM6].enr_reg == NULL)  dtm_clks[CLK_TIM6].enr_reg  = &RCC->APB1ENR;
    if (dtm_clks[CLK_TIM7].enr_reg == NULL)  dtm_clks[CLK_TIM7].enr_reg  = &RCC->APB1ENR;
    if (dtm_clks[CLK_TIM8].enr_reg == NULL)  dtm_clks[CLK_TIM8].enr_reg  = &RCC->APB2ENR;
    if (dtm_clks[CLK_TIM9].enr_reg == NULL)  dtm_clks[CLK_TIM9].enr_reg  = &RCC->APB2ENR;
    if (dtm_clks[CLK_TIM10].enr_reg == NULL) dtm_clks[CLK_TIM10].enr_reg = &RCC->APB2ENR;
    if (dtm_clks[CLK_TIM11].enr_reg == NULL) dtm_clks[CLK_TIM11].enr_reg = &RCC->APB2ENR;
    if (dtm_clks[CLK_TIM12].enr_reg == NULL) dtm_clks[CLK_TIM12].enr_reg = &RCC->APB1ENR;
    if (dtm_clks[CLK_TIM13].enr_reg == NULL) dtm_clks[CLK_TIM13].enr_reg = &RCC->APB1ENR;
    if (dtm_clks[CLK_TIM14].enr_reg == NULL) dtm_clks[CLK_TIM14].enr_reg = &RCC->APB1ENR;

    if (dtm_clks[CLK_SPI1].enr_reg == NULL) dtm_clks[CLK_SPI1].enr_reg = &RCC->APB2ENR;
    if (dtm_clks[CLK_SPI2].enr_reg == NULL) dtm_clks[CLK_SPI2].enr_reg = &RCC->APB1ENR;
    if (dtm_clks[CLK_SPI3].enr_reg == NULL) dtm_clks[CLK_SPI3].enr_reg = &RCC->APB1ENR;
}

static const char *const g_dtm_clk_arm_m4_st_gd_compatibles[] = {
    "puppy,clk-arm-m4-st-gd",
    "st,stm32f4-rcc",
};

const dtm_of_driver_t g_dtm_clk_arm_m4_st_gd_of_driver = {
    "clk-arm-m4-st-gd",
    g_dtm_clk_arm_m4_st_gd_compatibles,
    DTM_ARRAY_SIZE(g_dtm_clk_arm_m4_st_gd_compatibles),
    dtm_clk_arm_m4_st_gd_of_probe,
};

/* PLL时钟操作函数 */
static uint32_t pll_clk_get_rate(const struct clk_desc *clk)
{
    // 如果是PLL_VCO、PLL_P或PLL_Q节点，使用CLK_PLL的参数进行计算
    if (clk == &dtm_clks[CLK_PLL_VCO] || clk == &dtm_clks[CLK_PLL_P] || clk == &dtm_clks[CLK_PLL_Q]) {
        const struct clk_desc *pll_clk = &dtm_clks[CLK_PLL];
        const struct clk_desc *parent = &dtm_clks[pll_clk->parent];
        uint32_t parent_rate = parent->ops->get_rate(parent);
        uint32_t vco = parent_rate * pll_clk->plln / pll_clk->pllm;
        
        if (clk == &dtm_clks[CLK_PLL_VCO]) {
            return vco;
        } else if (clk == &dtm_clks[CLK_PLL_P]) {
            return vco / pll_clk->pllp;
        } else if (clk == &dtm_clks[CLK_PLL_Q]) {
            return vco / pll_clk->pllq;
        }
    }
    
    return 0;
}

static int pll_clk_enable(struct clk_desc *clk)
{
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)); /* 等待PLL就绪 */
    clk->enabled = 1;
    return 0;
}

static int pll_clk_disable(struct clk_desc *clk)
{
    RCC->CR &= ~RCC_CR_PLLON;
    clk->enabled = 0;
    return 0;
}

static int pll_clk_set_parent(struct clk_desc *clk, int parent_id)
{
    if (parent_id == CLK_HSI || parent_id == CLK_HSE) {
        clk->parent = parent_id;
        /* 更新PLL时钟源选择 */
        if (parent_id == CLK_HSI) {
            RCC->PLLCFGR &= ~(1 << 22); /* 选择HSI作为PLL时钟源 */
        } else {
            RCC->PLLCFGR |= (1 << 22);  /* 选择HSE作为PLL时钟源 */
        }
        return 0;
    }
    return -1;
}

static int pll_clk_set_rate(struct clk_desc *clk, uint32_t rate)
{
    /* PLL频率设置需要重新计算plln, pllm, pllp等参数，这里简化处理 */
    return -1;
}

static const struct clk_ops pll_clk_ops = {
    .get_rate = pll_clk_get_rate,
    .enable = pll_clk_enable,
    .disable = pll_clk_disable,
    .set_parent = pll_clk_set_parent,
    .set_rate = pll_clk_set_rate,
};

/* 分频时钟操作函数 */
static uint32_t div_clk_get_rate(const struct clk_desc *clk)
{
    const struct clk_desc *parent = &dtm_clks[clk->parent];
    uint32_t parent_rate = parent->ops->get_rate(parent);
    return parent_rate / clk->div;
}

static int div_clk_enable(struct clk_desc *clk)
{
    clk->enabled = 1;
    return 0;
}

static int div_clk_disable(struct clk_desc *clk)
{
    clk->enabled = 0;
    return 0;
}

static int div_clk_set_parent(struct clk_desc *clk, int parent_id)
{
    clk->parent = parent_id;
    return 0;
}

static int div_clk_set_rate(struct clk_desc *clk, uint32_t rate)
{
    const struct clk_desc *parent = &dtm_clks[clk->parent];
    uint32_t parent_rate = parent->ops->get_rate(parent);
    uint32_t new_div = parent_rate / rate;
    
    if (new_div > 0 && parent_rate % new_div == 0) {
        clk->div = new_div;
        return 0;
    }
    return -1;
}

static const struct clk_ops div_clk_ops = {
    .get_rate = div_clk_get_rate,
    .enable = div_clk_enable,
    .disable = div_clk_disable,
    .set_parent = div_clk_set_parent,
    .set_rate = div_clk_set_rate,
};

/* 寄存器位定义 */
#define RCC_CR_HSION      (1 << 0)
#define RCC_CR_HSEON      (1 << 16)
#define RCC_CR_HSERDY     (1 << 17)
#define RCC_CR_PLLON      (1 << 24)
#define RCC_CR_PLLRDY     (1 << 25)

#define RCC_CFGR_SW       (3 << 0)  // 系统时钟选择
#define RCC_CFGR_SW_HSI   (0 << 0)
#define RCC_CFGR_SW_HSE   (1 << 0)
#define RCC_CFGR_SW_PLL   (2 << 0)

#define RCC_CFGR_SWS      (3 << 2)  // 系统时钟状态
#define RCC_CFGR_SWS_HSI  (0 << 2)
#define RCC_CFGR_SWS_HSE  (1 << 2)
#define RCC_CFGR_SWS_PLL  (2 << 2)

/* 系统时钟操作函数 */
static uint32_t sysclk_get_rate(const struct clk_desc *clk)
{
    uint32_t sysclk_src = (RCC->CFGR >> 2) & 0x3;
    
    switch (sysclk_src) {
        case 0: /* HSI */
            return dtm_clks[CLK_HSI].ops->get_rate(&dtm_clks[CLK_HSI]);
        case 1: /* HSE */
            return dtm_clks[CLK_HSE].ops->get_rate(&dtm_clks[CLK_HSE]);
        case 2: /* PLL */
            return dtm_clks[CLK_PLL_P].ops->get_rate(&dtm_clks[CLK_PLL_P]);
        default:
            return 0;
    }
}

static int sysclk_set_parent(struct clk_desc *clk, int parent_id)
{
    uint32_t sw_val;
    
    switch (parent_id) {
        case CLK_HSI:
            sw_val = RCC_CFGR_SW_HSI;
            break;
        case CLK_HSE:
            sw_val = RCC_CFGR_SW_HSE;
            break;
        case CLK_PLL_P:
            sw_val = RCC_CFGR_SW_PLL;
            break;
        default:
            return -1;
    }
    
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | sw_val;
    
    /* 等待时钟切换完成 */
    while (((RCC->CFGR >> 2) & 0x3) != (sw_val >> 0));
    
    clk->parent = parent_id;
    return 0;
}

static const struct clk_ops sysclk_ops = {
    .get_rate = sysclk_get_rate,
    .enable = div_clk_enable,
    .disable = div_clk_disable,
    .set_parent = sysclk_set_parent,
    .set_rate = div_clk_set_rate,
};

/* 外设时钟操作函数 */
static uint32_t peri_clk_get_rate(const struct clk_desc *clk)
{
    const struct clk_desc *parent = &dtm_clks[clk->parent];
    return parent->ops->get_rate(parent);
}

static int peri_clk_enable(struct clk_desc *clk)
{
    if (clk->enr_reg && clk->enr_bit) {
        *clk->enr_reg |= clk->enr_bit;
        clk->enabled = 1;
        return 0;
    }
    return -1;
}

static int peri_clk_disable(struct clk_desc *clk)
{
    if (clk->enr_reg && clk->enr_bit) {
        *clk->enr_reg &= ~clk->enr_bit;
        clk->enabled = 0;
        return 0;
    }
    return -1;
}

static const struct clk_ops peri_clk_ops = {
    .get_rate = peri_clk_get_rate,
    .enable = peri_clk_enable,
    .disable = peri_clk_disable,
    .set_parent = div_clk_set_parent,
    .set_rate = div_clk_set_rate,
};

/* 定时器时钟操作函数 */
/* STM32F4 规范：当 APBx 预分频器 != 1 时，定时器输入时钟 = PCLKx × 2 */
static uint32_t timer_clk_get_rate(const struct clk_desc *clk)
{
    const struct clk_desc *parent = &dtm_clks[clk->parent];
    uint32_t parent_rate = parent->ops->get_rate(parent);
    if (parent->div > 1) {
        return parent_rate * 2;
    }
    return parent_rate;
}

static const struct clk_ops timer_clk_ops = {
    .get_rate = timer_clk_get_rate,
    .enable = peri_clk_enable,
    .disable = peri_clk_disable,
    .set_parent = div_clk_set_parent,
    .set_rate = div_clk_set_rate,
};

/* 静态时钟树数组定义 */
struct clk_desc dtm_clks[CLK_COUNT] = {
    [CLK_HSI] = {
        .name = "hsi",
        .freq = 16000000,  // HSI 固定频率 16MHz
        .parent = -1,      // 无父节点
        .ops = &fixed_clk_ops,
        .enabled = 0,
    },
    [CLK_HSE] = {
        .name = "hse",
        .freq = DTM_HSE_FREQ,  // HSE 频率由宏定义
        .parent = -1,         // 无父节点
        .ops = &fixed_clk_ops,
        .enabled = 0,
    },
    [CLK_PLL] = {
        .name = "pll",
        .parent = CLK_HSE,    // 默认父节点为 HSE
        .ops = &pll_clk_ops,
        .plln = 384,          // 默认 PLL 倍频系数
        .pllm = 8,            // 默认 PLL 预分频系数
        .pllp = 4,            // 默认 PLL P分频系数
        .pllq = 8,            // 默认 PLL Q分频系数
        .enabled = 0,
    },
    [CLK_PLL_VCO] = {
        .name = "pll_vco",
        .parent = CLK_PLL,
        .ops = &pll_clk_ops,
        .enabled = 0,
    },
    [CLK_PLL_P] = {
        .name = "pll_p",
        .parent = CLK_PLL,
        .ops = &pll_clk_ops,
        .enabled = 0,
    },
    [CLK_PLL_Q] = {
        .name = "pll_q",
        .parent = CLK_PLL,
        .ops = &pll_clk_ops,
        .enabled = 0,
    },
    [CLK_SYSCLK] = {
        .name = "sysclk",
        .parent = CLK_PLL_P,  // 默认父节点为 PLL_P
        .ops = &sysclk_ops,
        .enabled = 1,
    },
    [CLK_HCLK] = {
        .name = "hclk",
        .parent = CLK_SYSCLK,
        .ops = &div_clk_ops,
        .div = 1,             // AHB 分频系数，默认1
        .enabled = 1,
    },
    [CLK_PCLK1] = {
        .name = "pclk1",
        .parent = CLK_HCLK,
        .ops = &div_clk_ops,
        .div = 2,             // APB1 分频系数，默认4
        .enabled = 1,
    },
    [CLK_PCLK2] = {
        .name = "pclk2",
        .parent = CLK_HCLK,
        .ops = &div_clk_ops,
        .div = 1,             // APB2 分频系数，默认2
        .enabled = 1,
    },
    /* GPIO 时钟节点 */
    [CLK_GPIOA] = {
        .name = "gpioa",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(AHB1ENR),
        .enr_bit = 1 << 0,    // GPIOA 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOB] = {
        .name = "gpiob",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(AHB1ENR),
        .enr_bit = 1 << 1,    // GPIOB 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOC] = {
        .name = "gpioc",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(AHB1ENR),
        .enr_bit = 1 << 2,    // GPIOC 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOD] = {
        .name = "gpiod",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(AHB1ENR),
        .enr_bit = 1 << 3,    // GPIOD 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOE] = {
        .name = "gpioe",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(AHB1ENR),
        .enr_bit = 1 << 4,    // GPIOE 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOF] = {
        .name = "gpiof",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(AHB1ENR),
        .enr_bit = 1 << 5,    // GPIOF 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOG] = {
        .name = "gpiog",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(AHB1ENR),
        .enr_bit = 1 << 6,    // GPIOG 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOH] = {
        .name = "gpioh",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(AHB1ENR),
        .enr_bit = 1 << 7,    // GPIOH 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOI] = {
        .name = "gpioi",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(AHB1ENR),
        .enr_bit = 1 << 8,    // GPIOI 时钟使能位
        .enabled = 0,
    },
    /* UART 时钟节点 */
    [CLK_UART1] = {
        .name = "uart1",
        .parent = CLK_PCLK2,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB2ENR),
        .enr_bit = 1 << 4,    // UART1 时钟使能位
        .enabled = 0,
    },
    [CLK_UART2] = {
        .name = "uart2",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB1ENR),
        .enr_bit = 1 << 17,   // UART2 时钟使能位
        .enabled = 0,
    },
    [CLK_UART3] = {
        .name = "uart3",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB1ENR),
        .enr_bit = 1 << 18,   // UART3 时钟使能位
        .enabled = 0,
    },
    [CLK_UART4] = {
        .name = "uart4",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB1ENR),
        .enr_bit = 1 << 19,   // UART4 时钟使能位
        .enabled = 0,
    },
    [CLK_UART5] = {
        .name = "uart5",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB1ENR),
        .enr_bit = 1 << 20,   // UART5 时钟使能位
        .enabled = 0,
    },
    [CLK_UART6] = {
        .name = "uart6",
        .parent = CLK_PCLK2,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB2ENR),
        .enr_bit = 1 << 5,    // UART6 时钟使能位
        .enabled = 0,
    },
    [CLK_UART7] = {
        .name = "uart7",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB1ENR),
        .enr_bit = 1 << 30,   // UART7 时钟使能位
        .enabled = 0,
    },
    [CLK_UART8] = {
        .name = "uart8",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB1ENR),
        .enr_bit = 1 << 31,   // UART8 时钟使能位
        .enabled = 0,
    },
    /* Timer 时钟节点 */
    [CLK_TIM1] = {
        .name = "tim1",
        .parent = CLK_PCLK2,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 0,    // TIM1 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM2] = {
        .name = "tim2",
        .parent = CLK_PCLK1,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 0,    // TIM2 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM3] = {
        .name = "tim3",
        .parent = CLK_PCLK1,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 1,    // TIM3 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM4] = {
        .name = "tim4",
        .parent = CLK_PCLK1,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 2,    // TIM4 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM5] = {
        .name = "tim5",
        .parent = CLK_PCLK1,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 3,    // TIM5 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM6] = {
        .name = "tim6",
        .parent = CLK_PCLK1,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 4,    // TIM6 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM7] = {
        .name = "tim7",
        .parent = CLK_PCLK1,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 5,    // TIM7 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM8] = {
        .name = "tim8",
        .parent = CLK_PCLK2,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 1,    // TIM8 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM9] = {
        .name = "tim9",
        .parent = CLK_PCLK2,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 16,   // TIM9 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM10] = {
        .name = "tim10",
        .parent = CLK_PCLK2,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 17,   // TIM10 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM11] = {
        .name = "tim11",
        .parent = CLK_PCLK2,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 18,   // TIM11 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM12] = {
        .name = "tim12",
        .parent = CLK_PCLK1,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 6,    // TIM12 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM13] = {
        .name = "tim13",
        .parent = CLK_PCLK1,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 7,    // TIM13 时钟使能位
        .enabled = 0,
    },
    [CLK_TIM14] = {
        .name = "tim14",
        .parent = CLK_PCLK1,
        .ops = &timer_clk_ops,
        .enr_bit = 1 << 8,    // TIM14 时钟使能位
        .enabled = 0,
    },
    /* SPI 时钟节点 */
    [CLK_SPI1] = {
        .name = "spi1",
        .parent = CLK_PCLK2,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB2ENR),
        .enr_bit = 1 << 12,   // SPI1 时钟使能位
        .enabled = 0,
    },
    [CLK_SPI2] = {
        .name = "spi2",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB1ENR),
        .enr_bit = 1 << 14,   // SPI2 时钟使能位
        .enabled = 0,
    },
    [CLK_SPI3] = {
        .name = "spi3",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = RCC_INIT_REG(APB1ENR),
        .enr_bit = 1 << 15,   // SPI3 时钟使能位
        .enabled = 0,
    },
};

/* 全局时钟变量（保持向后兼容） */
uint32_t sysclk = 0, pll_vco = 0, pll_p_ck = 0, pll_q_ck = 0, hclk = 0, pclk1 = 0, pclk2 = 0;

/**
 * @brief       Get prescaler bits for RCC_CFGR register
 * @param       div: Divider value (1, 2, 4, 8, 16)
 * @retval      Prescaler bits for RCC_CFGR register
 */
static uint32_t get_prescaler_bits(uint32_t div)
{
    switch (div) {
        case 1: return 0x0;  // 0xx: AHB clock not divided
        case 2: return 0x4;  // 100: AHB clock divided by 2
        case 4: return 0x5;  // 101: AHB clock divided by 4
        case 8: return 0x6;  // 110: AHB clock divided by 8
        case 16: return 0x7; // 111: AHB clock divided by 16
        default: return 0x0; // Default to no division
    }
}

/**
 * @brief       Clock setting function
 * @note
 *
 *              Fvco: VCO frequency
 *              Fsys: System clock frequency, also the output clock frequency after main PLL p division
 *              Fq:   Main PLL q division output clock frequency
 *              Fs:   Main PLL input clock frequency, can be HSI, HSE, etc.
 *              Fvco = Fs * (plln / pllm);
 *              Fsys = Fvco / pllp = Fs * (plln / (pllm * pllp));
 *              Fq   = Fvco / pllq = Fs * (plln / (pllm * pllq));
 *
 *              When the external crystal is 8M, recommended values: plln = 336, pllm = 8, pllp = 2, pllq = 7.
 *              Get: Fvco = 8 * (336 / 8) = 336Mhz
 *                   Fsys = pll_p_ck = 336 / 2 = 168Mhz
 *                   Fq   = pll_q_ck = 336 / 7 = 48Mhz
 *
 *              The default frequencies that need to be configured for F407 are as follows:
 *              CPU frequency (HCLK) = pll_p_ck = 168Mhz
 *              AHB1/2/3(rcc_hclk1/2/3) = 168Mhz
 *              APB1(rcc_pclk1) = pll_p_ck / 4 = 42Mhz
 *              APB2(rcc_pclk2) = pll_p_ck / 2 = 84Mhz
 *
 * @retval      Error code: 0, success; 1, HSE error; 2, PLL1 error; 3, PLL2 error; 4, clock switch error;
 */
uint8_t sys_clock_set(void)
{
    uint8_t retval = 0;
    uint32_t plln, pllm, pllp, pllq;

    /* 从时钟树中获取 PLL 参数 */
    plln = dtm_clks[CLK_PLL].plln;
    pllm = dtm_clks[CLK_PLL].pllm;
    pllp = dtm_clks[CLK_PLL].pllp;
    pllq = dtm_clks[CLK_PLL].pllq;

    /* 使能 HSE */
    if (dtm_clks[CLK_HSE].ops->enable(&dtm_clks[CLK_HSE]) != 0)
    {
        retval = 1;     /* HSE 使能失败 */
    }
    else
    {
        RCC->APB1ENR |= 1 << 28;                /* Enable power interface clock */

        /* 配置 PLL 寄存器 */
        RCC->PLLCFGR = 0; /* 先清零 */
        RCC->PLLCFGR |= 0X3F & pllm;            /* Set main PLL predivision factor,  PLLM[5:0]: 2~63 */
        RCC->PLLCFGR |= plln << 6;              /* Set main PLL multiplication factor,    PLLN[8:0]: 192~432 */
        RCC->PLLCFGR |= ((pllp >> 1) - 1) << 16;/* Set main PLL p division factor, PLLP[1:0]: 0~3, representing 2~8 division */
        RCC->PLLCFGR |= pllq << 24;             /* Set main PLL q division factor, PLLQ[3:0]: 2~15 */
        RCC->PLLCFGR |= 1 << 22;                /* Set main PLL clock source from HSE */

        /* 配置 AHB/APB 分频 */
        uint32_t hpre_div = dtm_clks[CLK_HCLK].div;
        uint32_t ppre1_div = dtm_clks[CLK_PCLK1].div;
        uint32_t ppre2_div = dtm_clks[CLK_PCLK2].div;
        
        RCC->CFGR = (RCC->CFGR & ~(0xF << 4)) | (get_prescaler_bits(hpre_div) << 4);   /* HPRE[3:0] */
        RCC->CFGR = (RCC->CFGR & ~(0x7 << 10)) | (get_prescaler_bits(ppre1_div) << 10); /* PPRE1[2:0] */
        RCC->CFGR = (RCC->CFGR & ~(0x7 << 13)) | (get_prescaler_bits(ppre2_div) << 13); /* PPRE2[2:0] */

        /* 使能 PLL */
        if (dtm_clks[CLK_PLL].ops->enable(&dtm_clks[CLK_PLL]) != 0)
        {
            retval = 2;     /* PLL 使能失败 */
        }
        else
        {
            /* 切换系统时钟到 PLL */
            if (dtm_clks[CLK_SYSCLK].ops->set_parent(&dtm_clks[CLK_SYSCLK], CLK_PLL_P) != 0)
            {
                retval = 4; /* 时钟切换失败 */
            }
        }
    }

    return retval;
}

/**
 * @brief       System clock initialization function
 * @retval      None
 */
void sys_stm32_clock_init(void)
{
    /* 重置 RCC 寄存器 */
    RCC->CR = 0x00000001;           /* Set HISON, enable internal high-speed RC oscillator, clear other bits */
    RCC->CFGR = 0x00000000;         /* Clear CFGR */
    RCC->PLLCFGR = 0x00000000;      /* Clear PLLCFGR */
    RCC->CIR = 0x00000000;          /* Clear CIR */
    
    /* 使用静态时钟树中的参数，不再需要外部传入 */
    
    sys_clock_set();  /* Set clock */
}

/**
 * @brief       dump the clock tree by printing the current clock configuration
 * @note        This function prints the current clock configuration in a tree format.
 * @retval      None
 */
void dtm_clk_dumptree(void)
{
    /* 使用新的时钟树结构获取时钟频率 */
    uint32_t hsi_freq = dtm_clks[CLK_HSI].ops->get_rate(&dtm_clks[CLK_HSI]);
    uint32_t hse_freq = dtm_clks[CLK_HSE].ops->get_rate(&dtm_clks[CLK_HSE]);
    uint32_t pll_vco_freq = dtm_clks[CLK_PLL_VCO].ops->get_rate(&dtm_clks[CLK_PLL_VCO]);
    uint32_t pll_p_freq = dtm_clks[CLK_PLL_P].ops->get_rate(&dtm_clks[CLK_PLL_P]);
    uint32_t pll_q_freq = dtm_clks[CLK_PLL_Q].ops->get_rate(&dtm_clks[CLK_PLL_Q]);
    uint32_t sysclk_freq = dtm_clks[CLK_SYSCLK].ops->get_rate(&dtm_clks[CLK_SYSCLK]);
    uint32_t hclk_freq = dtm_clks[CLK_HCLK].ops->get_rate(&dtm_clks[CLK_HCLK]);
    uint32_t pclk1_freq = dtm_clks[CLK_PCLK1].ops->get_rate(&dtm_clks[CLK_PCLK1]);
    uint32_t pclk2_freq = dtm_clks[CLK_PCLK2].ops->get_rate(&dtm_clks[CLK_PCLK2]);

    /* 更新全局变量以保持向后兼容 */
    sysclk = sysclk_freq;
    pll_vco = pll_vco_freq;
    pll_p_ck = pll_p_freq;
    pll_q_ck = pll_q_freq;
    hclk = hclk_freq;
    pclk1 = pclk1_freq;
    pclk2 = pclk2_freq;

    /* 打印时钟树 */
    dtm_printf("clock-tree\n");
    dtm_printf("├── HSE (%lu Hz) [%s]\n", hse_freq, dtm_clks[CLK_HSE].enabled ? "enabled" : "disabled");
    dtm_printf("│   └── PLL\n");
    dtm_printf("│       ├── VCO (%lu Hz)\n", pll_vco_freq);
    dtm_printf("│       ├── PLL_P (SYSCLK, %lu Hz)\n", pll_p_freq);
    dtm_printf("│       │   └── AHB (HCLK, %lu Hz)\n", hclk_freq);
    dtm_printf("│       │       ├── APB1 (PCLK1, %lu Hz)\n", pclk1_freq);
    dtm_printf("│       │       └── APB2 (PCLK2, %lu Hz)\n", pclk2_freq);
    dtm_printf("│       └── PLL_Q (USB, %lu Hz)\n", pll_q_freq);
    dtm_printf("├── HSI (%lu Hz) [%s]\n", hsi_freq, dtm_clks[CLK_HSI].enabled ? "enabled" : "disabled");
    dtm_printf("└── LSE/LSE not shown\n");

    /* 打印时钟树配置信息 */
    dtm_printf("\nClock Configuration:\n");
    dtm_printf("PLL: N=%lu, M=%lu, P=%lu, Q=%lu\n", 
              dtm_clks[CLK_PLL].plln, 
              dtm_clks[CLK_PLL].pllm, 
              dtm_clks[CLK_PLL].pllp, 
              dtm_clks[CLK_PLL].pllq);
    dtm_printf("SYSCLK source: %s\n", 
              dtm_clks[CLK_SYSCLK].parent == CLK_HSI ? "HSI" : 
              dtm_clks[CLK_SYSCLK].parent == CLK_HSE ? "HSE" : "PLL");
    dtm_printf("AHB divider: %lu\n", dtm_clks[CLK_HCLK].div);
    dtm_printf("APB1 divider: %lu\n", dtm_clks[CLK_PCLK1].div);
    dtm_printf("APB2 divider: %lu\n", dtm_clks[CLK_PCLK2].div);
}

void dtm_clk_init(void)
{
    pup_dtm_clock_config_t board_clock;

    if (dtm_board_get_clock(&board_clock) == 0) {
        if (board_clock.base != 0U) {
            g_rcc_base = board_clock.base;
        }
        if (board_clock.hse_frequency != 0U) {
            dtm_clks[CLK_HSE].freq = board_clock.hse_frequency;
        }
        if (board_clock.pll_m != 0U) {
            dtm_clks[CLK_PLL].pllm = board_clock.pll_m;
        }
        if (board_clock.pll_n != 0U) {
            dtm_clks[CLK_PLL].plln = board_clock.pll_n;
        }
        if (board_clock.pll_p != 0U) {
            dtm_clks[CLK_PLL].pllp = board_clock.pll_p;
        }
        if (board_clock.pll_q != 0U) {
            dtm_clks[CLK_PLL].pllq = board_clock.pll_q;
        }
        if (board_clock.hclk_divider != 0U) {
            dtm_clks[CLK_HCLK].div = board_clock.hclk_divider;
        }
        if (board_clock.pclk1_divider != 0U) {
            dtm_clks[CLK_PCLK1].div = board_clock.pclk1_divider;
        }
        if (board_clock.pclk2_divider != 0U) {
            dtm_clks[CLK_PCLK2].div = board_clock.pclk2_divider;
        }
    }

    dtm_clk_apply_default_register_refs();

    /* Initialize the system clock to 168MHz */
    sys_stm32_clock_init(); /* Set PLL1 parameters for 168MHz system clock */

    /* 不再静态使能GPIO和UART时钟，将通过时钟树API动态使能 */
}

/* 公共时钟API函数 */

/**
 * @brief 获取时钟频率
 * @param clk_id 时钟ID
 * @return 时钟频率，单位Hz
 */
uint32_t clk_get_rate(int clk_id)
{
    if (clk_id >= 0 && clk_id < CLK_COUNT) {
        return dtm_clks[clk_id].ops->get_rate(&dtm_clks[clk_id]);
    }
    return 0;
}

/**
 * @brief 使能时钟
 * @param clk_id 时钟ID
 * @return 0表示成功，其他表示失败
 */
int clk_enable(int clk_id)
{
    if (clk_id >= 0 && clk_id < CLK_COUNT) {
        dtm_clk_apply_default_register_refs();
        return dtm_clks[clk_id].ops->enable(&dtm_clks[clk_id]);
    }
    return -1;
}

/**
 * @brief 禁用时钟
 * @param clk_id 时钟ID
 * @return 0表示成功，其他表示失败
 */
int clk_disable(int clk_id)
{
    if (clk_id >= 0 && clk_id < CLK_COUNT) {
        dtm_clk_apply_default_register_refs();
        return dtm_clks[clk_id].ops->disable(&dtm_clks[clk_id]);
    }
    return -1;
}

/**
 * @brief 设置时钟父节点
 * @param clk_id 时钟ID
 * @param parent_id 父时钟ID
 * @return 0表示成功，其他表示失败
 */
int clk_set_parent(int clk_id, int parent_id)
{
    if (clk_id >= 0 && clk_id < CLK_COUNT && 
        parent_id >= -1 && parent_id < CLK_COUNT) {
        return dtm_clks[clk_id].ops->set_parent(&dtm_clks[clk_id], parent_id);
    }
    return -1;
}

/**
 * @brief 设置时钟频率
 * @param clk_id 时钟ID
 * @param rate 目标频率
 * @return 0表示成功，其他表示失败
 */
int clk_set_rate(int clk_id, uint32_t rate)
{
    if (clk_id >= 0 && clk_id < CLK_COUNT) {
        return dtm_clks[clk_id].ops->set_rate(&dtm_clks[clk_id], rate);
    }
    return -1;
}

#endif /* DTM_BINFDING_CLK_ARM_M4_ST_GD */
