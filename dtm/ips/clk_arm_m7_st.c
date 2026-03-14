
#include "dtm_bindings.h"

#ifdef DTM_BINFDING_CLK_ARM_M7_ST

#include <stdint.h>
#include "../common/dtm_clk.h"
#include "../common/dtm_uart.h"

/* RCC寄存器结构定义 */
typedef struct
{
    volatile uint32_t CR;             /*!< RCC clock control register,                                              Address offset: 0x00  */
    volatile uint32_t HSICFGR;        /*!< HSI Clock Calibration Register,                                          Address offset: 0x04  */
    volatile uint32_t CRRCR;          /*!< Clock Recovery RC  Register,                                             Address offset: 0x08  */
    volatile uint32_t CSICFGR;        /*!< CSI Clock Calibration Register,                                          Address offset: 0x0C  */
    volatile uint32_t CFGR;           /*!< RCC clock configuration register,                                        Address offset: 0x10  */
    uint32_t     RESERVED1;       /*!< Reserved,                                                                Address offset: 0x14  */
    volatile uint32_t D1CFGR;         /*!< RCC Domain 1 configuration register,                                     Address offset: 0x18  */
    volatile uint32_t D2CFGR;         /*!< RCC Domain 2 configuration register,                                     Address offset: 0x1C  */
    volatile uint32_t D3CFGR;         /*!< RCC Domain 3 configuration register,                                     Address offset: 0x20  */
    uint32_t     RESERVED2;       /*!< Reserved,                                                                Address offset: 0x24  */
    volatile uint32_t PLLCKSELR;      /*!< RCC PLLs Clock Source Selection Register,                                Address offset: 0x28  */
    volatile uint32_t PLLCFGR;        /*!< RCC PLLs  Configuration Register,                                        Address offset: 0x2C  */
    volatile uint32_t PLL1DIVR;       /*!< RCC PLL1 Dividers Configuration Register,                                Address offset: 0x30  */
    volatile uint32_t PLL1FRACR;      /*!< RCC PLL1 Fractional Divider Configuration Register,                      Address offset: 0x34  */
    volatile uint32_t PLL2DIVR;       /*!< RCC PLL2 Dividers Configuration Register,                                Address offset: 0x38  */
    volatile uint32_t PLL2FRACR;      /*!< RCC PLL2 Fractional Divider Configuration Register,                      Address offset: 0x3C  */
    volatile uint32_t PLL3DIVR;       /*!< RCC PLL3 Dividers Configuration Register,                                Address offset: 0x40  */
    volatile uint32_t PLL3FRACR;      /*!< RCC PLL3 Fractional Divider Configuration Register,                      Address offset: 0x44  */
    uint32_t      RESERVED3;      /*!< Reserved,                                                                Address offset: 0x48  */
    volatile uint32_t  D1CCIPR;       /*!< RCC Domain 1 Kernel Clock Configuration Register                         Address offset: 0x4C  */
    volatile uint32_t  D2CCIP1R;      /*!< RCC Domain 2 Kernel Clock Configuration Register                         Address offset: 0x50  */
    volatile uint32_t  D2CCIP2R;      /*!< RCC Domain 2 Kernel Clock Configuration Register                         Address offset: 0x54  */
    volatile uint32_t  D3CCIPR;       /*!< RCC Domain 3 Kernel Clock Configuration Register                         Address offset: 0x58  */
    uint32_t      RESERVED4;      /*!< Reserved,                                                                Address offset: 0x5C  */
    volatile uint32_t  CIER;          /*!< RCC Clock Source Interrupt Enable Register                               Address offset: 0x60  */
    volatile uint32_t  CIFR;          /*!< RCC Clock Source Interrupt Flag Register                                 Address offset: 0x64  */
    volatile uint32_t  CICR;          /*!< RCC Clock Source Interrupt Clear Register                                Address offset: 0x68  */
    uint32_t     RESERVED5;       /*!< Reserved,                                                                Address offset: 0x6C  */
    volatile uint32_t  BDCR;          /*!< RCC Vswitch Backup Domain Control Register,                              Address offset: 0x70  */
    volatile uint32_t  CSR;           /*!< RCC clock control & status register,                                     Address offset: 0x74  */
    uint32_t     RESERVED6;       /*!< Reserved,                                                                Address offset: 0x78  */
    volatile uint32_t AHB3RSTR;       /*!< RCC AHB3 peripheral reset register,                                      Address offset: 0x7C  */
    volatile uint32_t AHB1RSTR;       /*!< RCC AHB1 peripheral reset register,                                      Address offset: 0x80  */
    volatile uint32_t AHB2RSTR;       /*!< RCC AHB2 peripheral reset register,                                      Address offset: 0x84  */
    volatile uint32_t AHB4RSTR;       /*!< RCC AHB4 peripheral reset register,                                      Address offset: 0x88  */
    volatile uint32_t APB3RSTR;       /*!< RCC APB3 peripheral reset register,                                      Address offset: 0x8C  */
    volatile uint32_t APB1LRSTR;      /*!< RCC APB1 peripheral reset Low Word register,                             Address offset: 0x90  */
    volatile uint32_t APB1HRSTR;      /*!< RCC APB1 peripheral reset High Word register,                            Address offset: 0x94  */
    volatile uint32_t APB2RSTR;       /*!< RCC APB2 peripheral reset register,                                      Address offset: 0x98  */
    volatile uint32_t APB4RSTR;       /*!< RCC APB4 peripheral reset register,                                      Address offset: 0x9C  */
    volatile uint32_t GCR;            /*!< RCC RCC Global Control  Register,                                        Address offset: 0xA0  */
    uint32_t     RESERVED8;       /*!< Reserved,                                                                Address offset: 0xA4  */
    volatile uint32_t D3AMR;          /*!< RCC Domain 3 Autonomous Mode Register,                                   Address offset: 0xA8  */
    uint32_t     RESERVED11[9];    /*!< Reserved, 0xAC-0xCC                                                      Address offset: 0xAC  */
    volatile uint32_t RSR;            /*!< RCC Reset status register,                                               Address offset: 0xD0  */
    volatile uint32_t AHB3ENR;        /*!< RCC AHB3 peripheral clock  register,                                     Address offset: 0xD4  */
    volatile uint32_t AHB1ENR;        /*!< RCC AHB1 peripheral clock  register,                                     Address offset: 0xD8  */
    volatile uint32_t AHB2ENR;        /*!< RCC AHB2 peripheral clock  register,                                     Address offset: 0xDC  */
    volatile uint32_t AHB4ENR;        /*!< RCC AHB4 peripheral clock  register,                                     Address offset: 0xE0  */
    volatile uint32_t APB3ENR;        /*!< RCC APB3 peripheral clock  register,                                     Address offset: 0xE4  */
    volatile uint32_t APB1LENR;       /*!< RCC APB1 peripheral clock  Low Word register,                            Address offset: 0xE8  */
    volatile uint32_t APB1HENR;       /*!< RCC APB1 peripheral clock  High Word register,                           Address offset: 0xEC  */
    volatile uint32_t APB2ENR;        /*!< RCC APB2 peripheral clock  register,                                     Address offset: 0xF0  */
    volatile uint32_t APB4ENR;        /*!< RCC APB4 peripheral clock  register,                                     Address offset: 0xF4  */
    uint32_t      RESERVED12;      /*!< Reserved,                                                                Address offset: 0xF8  */
    volatile uint32_t AHB3LPENR;      /*!< RCC AHB3 peripheral sleep clock  register,                               Address offset: 0xFC  */
    volatile uint32_t AHB1LPENR;      /*!< RCC AHB1 peripheral sleep clock  register,                               Address offset: 0x100 */
    volatile uint32_t AHB2LPENR;      /*!< RCC AHB2 peripheral sleep clock  register,                               Address offset: 0x104 */
    volatile uint32_t AHB4LPENR;      /*!< RCC AHB4 peripheral sleep clock  register,                               Address offset: 0x108 */
    volatile uint32_t APB3LPENR;      /*!< RCC APB3 peripheral sleep clock  register,                               Address offset: 0x10C */
    volatile uint32_t APB1LLPENR;     /*!< RCC APB1 peripheral sleep clock  Low Word register,                      Address offset: 0x110 */
    volatile uint32_t APB1HLPENR;     /*!< RCC APB1 peripheral sleep clock  High Word register,                     Address offset: 0x114 */
    volatile uint32_t APB2LPENR;      /*!< RCC APB2 peripheral sleep clock  register,                               Address offset: 0x118 */
    volatile uint32_t APB4LPENR;      /*!< RCC APB4 peripheral sleep clock  register,                               Address offset: 0x11C */
    uint32_t     RESERVED13[4];   /*!< Reserved, 0x120-0x12C                                                    Address offset: 0x120 */
} DTM_RCC_Type;


#ifndef DTM_CLK_BASE
#define DTM_CLK_BASE 0x58024400UL
#endif

#define RCC ((DTM_RCC_Type *)DTM_CLK_BASE)


// const RCC_CFGR_SW_HSI = 0;
// const RCC_CFGR_SW_CSI = 1;
// const RCC_CFGR_SW_HSE = 2;
// const RCC_CFGR_SW_PLL1 = 3;

// const RCC_PLLSOURCE_HSI = 0b00;
// const RCC_PLLSOURCE_CSI = 0b01;
// const RCC_PLLSOURCE_HSE = 0b10;
// const RCC_PLLSOURCE_NONE = 0b11;

/* 寄存器位定义 */
#define RCC_CR_HSION      (1 << 0)
#define RCC_CR_HSEON      (1 << 16)
#define RCC_CR_HSERDY     (1 << 17)
#define RCC_CR_PLL1ON     (1 << 24)
#define RCC_CR_PLL1RDY    (1 << 25)

#define RCC_CFGR_SW       (3 << 0)  // 系统时钟选择
#define RCC_CFGR_SW_HSI   (0 << 0)
#define RCC_CFGR_SW_CSI   (1 << 0)
#define RCC_CFGR_SW_HSE   (2 << 0)
#define RCC_CFGR_SW_PLL1  (3 << 0)

#define RCC_CFGR_SWS      (0x38 << 0)  // 系统时钟状态
#define RCC_CFGR_SWS_HSI  (0x00 << 3)
#define RCC_CFGR_SWS_CSI  (0x08 << 3)
#define RCC_CFGR_SWS_HSE  (0x10 << 3)
#define RCC_CFGR_SWS_PLL1 (0x18 << 3)

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

/* PLL时钟操作函数 */
static uint32_t pll_clk_get_rate(const struct clk_desc *clk)
{
    // 如果是PLL_VCO、PLL_P或PLL_Q节点，使用CLK_PLL的参数进行计算
    if (clk == &dtm_clks[CLK_PLL_VCO] || clk == &dtm_clks[CLK_PLL_P] || clk == &dtm_clks[CLK_PLL_Q]) {
        const struct clk_desc *pll_clk = &dtm_clks[CLK_PLL];
        const struct clk_desc *parent = &dtm_clks[pll_clk->parent];
        uint32_t parent_rate = parent->ops->get_rate(parent);
        uint32_t vco = (uint64_t)parent_rate * pll_clk->plln / pll_clk->pllm;
        
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
    RCC->CR |= RCC_CR_PLL1ON;
    while (!(RCC->CR & RCC_CR_PLL1RDY)); /* 等待PLL就绪 */
    clk->enabled = 1;
    return 0;
}

static int pll_clk_disable(struct clk_desc *clk)
{
    RCC->CR &= ~RCC_CR_PLL1ON;
    clk->enabled = 0;
    return 0;
}

static int pll_clk_set_parent(struct clk_desc *clk, int parent_id)
{
    if (parent_id == CLK_HSI || parent_id == CLK_HSE) {
        clk->parent = parent_id;
        /* 更新PLL时钟源选择 */
        if (parent_id == CLK_HSI) {
            RCC->PLLCKSELR &= ~(0x3 << 0); /* 选择HSI作为PLL时钟源 */
        } else {
            RCC->PLLCKSELR |= (0x2 << 0);  /* 选择HSE作为PLL时钟源 */
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

/* 系统时钟操作函数 */
static uint32_t sysclk_get_rate(const struct clk_desc *clk)
{
    uint32_t sysclk_src = (RCC->CFGR & RCC_CFGR_SWS) >> 3;
    
    switch (sysclk_src) {
        case 0: /* HSI */
            return dtm_clks[CLK_HSI].ops->get_rate(&dtm_clks[CLK_HSI]);
        case 1: /* CSI */
            return 4000000; // CSI 默认 4MHz
        case 2: /* HSE */
            return dtm_clks[CLK_HSE].ops->get_rate(&dtm_clks[CLK_HSE]);
        case 3: /* PLL */
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
            sw_val = RCC_CFGR_SW_PLL1;
            break;
        default:
            return -1;
    }
    
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | sw_val;
    
    /* 等待时钟切换完成 */
    uint32_t sws_val;
    do {
        sws_val = (RCC->CFGR & RCC_CFGR_SWS) >> 3;
    } while (sws_val != (sw_val >> 0));
    
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

/* 静态时钟树数组定义 */
struct clk_desc dtm_clks[CLK_COUNT] = {
    [CLK_HSI] = {
        .name = "hsi",
        .freq = 64000000,  // M7 HSI 固定频率 64MHz
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
        .plln = 32,          // 默认 PLL 倍频系数
        .pllm = 2,            // 默认 PLL 预分频系数
        .pllp = 2,            // 默认 PLL P分频系数
        .pllq = 2,            // 默认 PLL Q分频系数
        .pllr = 2,            // 默认 PLL R分频系数
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
        .div = 1,             // AHB 分频系数，默认2
        .enabled = 1,
    },
    [CLK_PCLK1] = {
        .name = "pclk1",
        .parent = CLK_HCLK,
        .ops = &div_clk_ops,
        .div = 4,             // APB1 分频系数，默认2
        .enabled = 1,
    },
    [CLK_PCLK2] = {
        .name = "pclk2",
        .parent = CLK_HCLK,
        .ops = &div_clk_ops,
        .div = 2,             // APB2 分频系数，默认2
        .enabled = 1,
    },
    [CLK_PCLK3] = {
        .name = "pclk3",
        .parent = CLK_HCLK,
        .ops = &div_clk_ops,
        .div = 2,             // APB3 分频系数，默认2
        .enabled = 1,
    },
    [CLK_PCLK4] = {
        .name = "pclk4",
        .parent = CLK_HCLK,
        .ops = &div_clk_ops,
        .div = 2,             // APB4 分频系数，默认2
        .enabled = 1,
    },
    /* GPIO 时钟节点 - M7使用AHB4ENR */
    [CLK_GPIOA] = {
        .name = "gpioa",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->AHB4ENR,
        .enr_bit = 1 << 0,    // GPIOA 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOB] = {
        .name = "gpiob",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->AHB4ENR,
        .enr_bit = 1 << 1,    // GPIOB 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOC] = {
        .name = "gpioc",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->AHB4ENR,
        .enr_bit = 1 << 2,    // GPIOC 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOD] = {
        .name = "gpiod",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->AHB4ENR,
        .enr_bit = 1 << 3,    // GPIOD 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOE] = {
        .name = "gpioe",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->AHB4ENR,
        .enr_bit = 1 << 4,    // GPIOE 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOF] = {
        .name = "gpiof",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->AHB4ENR,
        .enr_bit = 1 << 5,    // GPIOF 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOG] = {
        .name = "gpiog",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->AHB4ENR,
        .enr_bit = 1 << 6,    // GPIOG 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOH] = {
        .name = "gpioh",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->AHB4ENR,
        .enr_bit = 1 << 7,    // GPIOH 时钟使能位
        .enabled = 0,
    },
    [CLK_GPIOI] = {
        .name = "gpioi",
        .parent = CLK_HCLK,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->AHB4ENR,
        .enr_bit = 1 << 8,    // GPIOI 时钟使能位
        .enabled = 0,
    },
    /* UART 时钟节点 */
    [CLK_UART1] = {
        .name = "uart1",
        .parent = CLK_PCLK2,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->APB2ENR,
        .enr_bit = 1 << 4,    // UART1 时钟使能位
        .enabled = 0,
    },
    [CLK_UART2] = {
        .name = "uart2",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->APB1LENR,
        .enr_bit = 1 << 17,   // UART2 时钟使能位
        .enabled = 0,
    },
    [CLK_UART3] = {
        .name = "uart3",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->APB1LENR,
        .enr_bit = 1 << 18,   // UART3 时钟使能位
        .enabled = 0,
    },
    [CLK_UART4] = {
        .name = "uart4",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->APB1LENR,
        .enr_bit = 1 << 19,   // UART4 时钟使能位
        .enabled = 0,
    },
    [CLK_UART5] = {
        .name = "uart5",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->APB1HENR,
        .enr_bit = 1 << 20,   // UART5 时钟使能位
        .enabled = 0,
    },
    [CLK_UART6] = {
        .name = "uart6",
        .parent = CLK_PCLK2,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->APB2ENR,
        .enr_bit = 1 << 5,    // UART6 时钟使能位
        .enabled = 0,
    },
    [CLK_UART7] = {
        .name = "uart7",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->APB1LENR,
        .enr_bit = 1 << 30,   // UART7 时钟使能位
        .enabled = 0,
    },
    [CLK_UART8] = {
        .name = "uart8",
        .parent = CLK_PCLK1,
        .ops = &peri_clk_ops,
        .enr_reg = &RCC->APB1HENR,
        .enr_bit = 1 << 31,   // UART8 时钟使能位
        .enabled = 0,
    },
};

/* 全局时钟变量（保持向后兼容） */
uint32_t sysclk = 0, pll_vco = 0, pll_p_ck = 0, pll_q_ck = 0, hclk = 0, pclk1 = 0, pclk2 = 0;

/**
 * @brief       Clock setting function
 * @note
 *              Fvco: VCO frequency
 *              Fsys: System clock frequency, also the output clock frequency after main PLL p division
 *              Fq:   Main PLL q division output clock frequency
 *              Fs:   Main PLL input clock frequency, can be HSI, HSE, etc.
 *              Fvco = Fs * (plln / pllm);
 *              Fsys = Fvco / pllp = Fs * (plln / (pllm * pllp));
 *              Fq   = Fvco / pllq = Fs * (plln / (pllm * pllq));
 * 
 *              D1CPRE Prescaler = 1, CPU clock frequency = Fsys
 *              HPRE Prescaler = 2, AHB clock frequency = Fsys / 2
 *              
 *
 *              When the external crystal is 8M, 480M recommended values: plln = 120, pllm = 1, pllp = 2, pllq = 2, pllr = 2.
 *              Get: Fvco = 8 * (120 / 1) = 960Mhz
 *                   Fsys = pll_p_ck = 960 / 2 = 480Mhz
 *                   Fq   = pll_q_ck = 960 / 2 = 480Mhz
 *
 *              The default frequencies that need to be configured for H743 are as follows:
 *              CPU frequency (HCLK) = pll_p_ck = 480Mhz
 *              AHB1/2(rcc_hclk1/2) = 480Mhz
 *              APB1(rcc_pclk1) = pll_p_ck / 4 = 42Mhz
 *              APB2(rcc_pclk2) = pll_p_ck / 2 = 84Mhz
 *
 * @retval      Error code: 0, success; 1, HSE error; 2, PLL1 error; 3, PLL2 error; 4, clock switch error;
 */
/* 获取AHB预分频器位值 (STM32H7: 0xxx=1, 1000=2, 1001=4, 1010=8, 1011=16, 1100=64, 1101=128, 1110=256, 1111=512) */
static uint32_t get_ahb_prescaler_bits(uint32_t div)
{
    switch(div) {
        case 1: return 0b0000;
        case 2: return 0b1000;
        case 4: return 0b1001;
        case 8: return 0b1010;
        case 16: return 0b1011;
        case 64: return 0b1100;
        case 128: return 0b1101;
        case 256: return 0b1110;
        case 512: return 0b1111;
        default: return 0b1000; // 默认2分频
    }
}

/* 获取APB预分频器位值 (STM32H7: 0xxx=1, 100=2, 101=4, 110=8, 111=16) */
static uint32_t get_apb_prescaler_bits(uint32_t div)
{
    switch(div) {
        case 1: return 0b000;
        case 2: return 0b100;
        case 4: return 0b101;
        case 8: return 0b110;
        case 16: return 0b111;
        default: return 0b100; // 默认2分频
    }
}

uint8_t sys_clock_set(void)
{
    uint32_t retry = 0;
    uint8_t retval = 0;
    uint8_t swsval = 0;
    
    /* 获取PLL参数 */
    uint32_t pllm = dtm_clks[CLK_PLL].pllm;
    uint32_t plln = dtm_clks[CLK_PLL].plln;
    uint32_t pllp = dtm_clks[CLK_PLL].pllp;
    uint32_t pllq = dtm_clks[CLK_PLL].pllq;
    uint32_t pllr = dtm_clks[CLK_PLL].pllr;

    RCC->CR |= 1 << 16; /* HSEON = 1, enable HSE */

    while (((RCC->CR & (1 << 17)) == 0) && (retry < 0X7FFF))
    {
        retry++;        /* Wait for HSE RDY */
    }

    if (retry == 0X7FFF)
    {
        retval = 1;     /* HSE not ready */
    }
    else
    {
        // RCC->APB1ENR |= 1 << 28;                /* Enable power interface clock */

        /* Set main PLL predivision factor,   DIVM1 PLLCKSELR[9:4]: 1~63 */
        RCC->PLLCKSELR &= ~(0x3F << 4);          /* Clear PLLM bits */
        RCC->PLLCKSELR |= pllm << 4;
        /* Set main PLL clock source,         PLLCKSELR[1:0]: 0b00: HSI, 0b01: CSI, 0b10: HSE, 0b11: reserved */
        RCC->PLLCKSELR &= ~(0b11 << 0);          /* Clear PLL source bits */
        RCC->PLLCKSELR |= 0b10 << 0;            /* Set main PLL clock source to HSE */
        
        /* Set main PLL VCO PLLCFGR[1] wide.  0: Wide VCO range: 192 to 836 MHz (default after reset) 1: Medium VCO range: 150 to 420 MHz */
        RCC->PLLCFGR &= ~(1 << 1);          /* Clear PLL VCO range bit */
        /* Set main PLL input ref range PLLCFGR[3:2] 11: The PLL1 input (ref1_ck) clock range frequency is between 8 and 16 MHz */
        RCC->PLLCFGR |= 0b11 << 2;          /* Set PLL input ref range to 8~16MHz */
        /* Clear PLL1FRACEN PLLCFGR[4] */
        RCC->PLLCFGR &= ~(1 << 4);          /* Clear PLL1 fractional enable bit */
        /* Enable main PLL P Q R PLLCFGR[18：16] */
        RCC->PLLCFGR |= (1 << 16) | (1 << 17) | (1 << 18); /* Enable main PLL P Q R division */

        /* Set main PLL multiplication factor, PLL1DIVR[8:0] 0x003: DIVN1 = 4 0x004: DIVN1 = 5 */
        RCC->PLL1DIVR &= ~(0x1FF << 0);         /* Clear PLLN bits */
        RCC->PLL1DIVR |= (plln - 1) << 0;            /* Set main PLL multiplication factor, PLLN[8:0]: 4~512 */
        /* Set main PLL p division factor, PLL1DIVR[15:9] 0000001: pll1_p_ck = vco1_ck / 2 0000011: pll1_p_ck = vco1_ck / 4 */
        RCC->PLL1DIVR &= ~(0x7F << 9);         /* Clear PLLP bits */
        RCC->PLL1DIVR |= (pllp - 1) << 9; /* Set main PLL p division factor, PLLP[6:0]: 2~128, must be a multiple of 2 */
        /* Set main PLL q division factor, PLL1DIVR[22:16] 0000001: pll1_q_ck = vco1_ck / 2 */
        RCC->PLL1DIVR &= ~(0x7F << 16);        /* Clear PLLQ bits */
        RCC->PLL1DIVR |= (pllq - 1) << 16; /* Set main PLL q division factor, PLLQ[6:0]: 2~128 */
        /* Set main PLL r division factor, PLL1DIVR[30:24] 0000001: pll1_r_ck = vco1_ck / 2 */
        RCC->PLL1DIVR &= ~(0x7F << 24);        /* Clear PLLR bits */
        RCC->PLL1DIVR |= (pllr - 1) << 24; /* Set main PLL r division factor, PLLR[6:0]: 2~128 */

        /* RCC Domain 1 Clock Configuration Register */
        /* Core prescaler not divided D1CFGR[11:8] 0xxx: sys_ck not divided (default after reset) 1000: sys_ck divided by 2 */
        RCC->D1CFGR &= ~(0xF << 8);            /* Clear D1CPRE bits */
        /* 配置AHB分频 */
        uint32_t hclk_div = dtm_clks[CLK_HCLK].div;
        RCC->D1CFGR = (RCC->D1CFGR & ~(0xF << 0)) | (get_ahb_prescaler_bits(hclk_div) << 0);  /* HPRE[3:0] */

        /* 配置APB1/2/3/4分频 */
        uint32_t pclk1_div = dtm_clks[CLK_PCLK1].div;
        uint32_t pclk2_div = dtm_clks[CLK_PCLK2].div;
        uint32_t pclk3_div = dtm_clks[CLK_PCLK3].div;
        uint32_t pclk4_div = dtm_clks[CLK_PCLK4].div;
        
        RCC->D2CFGR = (RCC->D2CFGR & ~(0x7 << 4)) | (get_apb_prescaler_bits(pclk1_div) << 4);  /* PPRE1[2:0] */
        RCC->D2CFGR = (RCC->D2CFGR & ~(0x7 << 8)) | (get_apb_prescaler_bits(pclk2_div) << 8);  /* PPRE2[2:0] */
        RCC->D1CFGR = (RCC->D1CFGR & ~(0x7 << 4)) | (get_apb_prescaler_bits(pclk3_div) << 4);  /* PPRE3[2:0] */
        RCC->D3CFGR = (RCC->D3CFGR & ~(0x7 << 4)) | (get_apb_prescaler_bits(pclk4_div) << 4);  /* PPRE4[2:0] */

        RCC->CR |= 1 << 24;                     /* Enable main PLL */

        retry = 0;
        while ((RCC->CR & (1 << 25)) == 0)      /* Wait for PLL ready */
        {
            retry++;

            if (retry > 0X1FFFFF)
            {
                retval = 2;                     /* Main PLL not ready */
                break;
            }
        }

        
        RCC->CFGR |= 3 << 0;                    /* Select main PLL as system clock */
        
        retry = 0;
        while (swsval != 3)                     /* Wait for successful switch of system clock source to pll_p_ck */
        {
            swsval = (RCC->CFGR & 0X38) >> 3;   /* Get SWS[2:0] status to check if switch is successful */
            retry++;

            if (retry > 0X1FFFFF)
            {
                retval = 4; /* Unable to switch clock */
                break;
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
    RCC->CR = 0x00000001;           /* Set HISON, enable internal high-speed RC oscillator, clear other bits */
    RCC->CFGR = 0x00000000;         /* Clear CFGR */
    RCC->PLLCFGR = 0x00000000;      /* Clear PLLCFGR */
    RCC->CIER = 0x00000000;          /* Clear CIR */
    
    sys_clock_set();  /* Set clock */
}

/**
 * @brief       dump the clock tree by printing the current clock configuration
 * @note        This function prints the current clock configuration in a tree format.
 * @retval      None
 */
void dtm_clk_dumptree(void)
{
    uint32_t hse = DTM_HSE_FREQ;
    // M7使用不同的寄存器结构，从PLLCKSELR和PLL1DIVR读取PLL参数
    uint32_t pllm = (RCC->PLLCKSELR >> 4) & 0x3F;         // DIVM1[5:0] in PLLCKSELR
    uint32_t plln = ((RCC->PLL1DIVR >> 0) & 0x1FF) + 1;   // DIVN1[8:0] in PLL1DIVR, +1是因为寄存器值是实际值减1
    uint32_t pllp = ((RCC->PLL1DIVR >> 9) & 0x7F) + 1;    // DIVP1[6:0] in PLL1DIVR, +1是因为寄存器值是实际值减1
    uint32_t pllq = ((RCC->PLL1DIVR >> 16) & 0x7F) + 1;   // DIVQ1[6:0] in PLL1DIVR, +1是因为寄存器值是实际值减1
    uint32_t sysclk_src = (RCC->CFGR >> 3) & 0x7;         // SWS[2:0] in CFGR, M7是3位
    
    // 从D1CFGR读取AHB预分频
    uint32_t hpre = (RCC->D1CFGR >> 0) & 0xF;             // HPRE[3:0] in D1CFGR
    // 从D2CFGR读取APB预分频
    uint32_t ppre1 = (RCC->D2CFGR >> 4) & 0x7;            // PPRE1[2:0] in D2CFGR
    uint32_t ppre2 = (RCC->D2CFGR >> 8) & 0x7;            // PPRE2[2:0] in D2CFGR
    
    // Calculate PLL VCO frequency
    pll_vco = (uint64_t)hse * plln / pllm;
    pll_p_ck = pll_vco / pllp;
    pll_q_ck = pll_vco / pllq;

    // Calculate SYSCLK
    if (sysclk_src == 0)
        sysclk = dtm_clks[CLK_HSI].freq; // HSI (M7默认64MHz)
    else if (sysclk_src == 1)
        sysclk = 4000000; // CSI (内部4MHz时钟)
    else if (sysclk_src == 2)
        sysclk = hse;      // HSE
    else if (sysclk_src == 3)
        sysclk = pll_p_ck; // PLL
    else
        sysclk = 0;

    // AHB prescaler table
    const uint16_t ahb_presc_tbl[16] = {1,1,1,1,1,1,1,1,2,4,8,16,64,128,256,512};
    // APB prescaler table
    const uint8_t apb_presc_tbl[8] = {1,1,1,1,2,4,8,16};

    hclk = sysclk / ahb_presc_tbl[hpre];
    pclk1 = hclk / apb_presc_tbl[ppre1];
    pclk2 = hclk / apb_presc_tbl[ppre2];

    dtm_printf("clock-tree\n");
    dtm_printf("├── HSE (%lu Hz)\n", hse);
    dtm_printf("│   └── PLL\n");
    dtm_printf("│       ├── VCO (%lu Hz)\n", pll_vco);
    dtm_printf("│       ├── PLL_P (SYSCLK, %lu Hz)\n", pll_p_ck);
    dtm_printf("│       │   └── AHB (HCLK, %lu Hz)\n", hclk);
    dtm_printf("│       │       ├── APB1 (PCLK1, %lu Hz)\n", pclk1);
    dtm_printf("│       │       └── APB2 (PCLK2, %lu Hz)\n", pclk2);
    dtm_printf("│       └── PLL_Q (USB, %lu Hz)\n", pll_q_ck);
    dtm_printf("├── HSI (16 MHz)\n");
    dtm_printf("└── LSE/LSE not shown\n");
}

/**
 * @brief       Initialize the system clock to 168MHz
 * @note        This function configures the system clock to 168MHz using the PLL.
 *              It also enables the clocks for GPIOA, GPIOF, USART1, and USART2.
 * @retval      None
 */
void dtm_clk_init(void)
{
    /* Initialize the system clock */
    sys_stm32_clock_init(); /* Set system clock */
}

/**
 * @brief       获取时钟频率
 * @param       clk_id: 时钟ID
 * @return      时钟频率(Hz), 失败返回0
 */
uint32_t clk_get_rate(int clk_id)
{
    if (clk_id >= 0 && clk_id < CLK_COUNT) {
        return dtm_clks[clk_id].ops->get_rate(&dtm_clks[clk_id]);
    }
    return 0;
}

/**
 * @brief       使能时钟
 * @param       clk_id: 时钟ID
 * @return      0表示成功, 其他表示失败
 */
int clk_enable(int clk_id)
{
    if (clk_id >= 0 && clk_id < CLK_COUNT) {
        return dtm_clks[clk_id].ops->enable(&dtm_clks[clk_id]);
    }
    return -1;
}

/**
 * @brief       禁用时钟
 * @param       clk_id: 时钟ID
 * @return      0表示成功, 其他表示失败
 */
int clk_disable(int clk_id)
{
    if (clk_id >= 0 && clk_id < CLK_COUNT) {
        return dtm_clks[clk_id].ops->disable(&dtm_clks[clk_id]);
    }
    return -1;
}

/**
 * @brief       设置时钟父节点
 * @param       clk_id: 时钟ID
 * @param       parent_id: 父时钟ID
 * @return      0表示成功, 其他表示失败
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
 * @brief       设置时钟频率
 * @param       clk_id: 时钟ID
 * @param       rate: 目标频率
 * @return      0表示成功, 其他表示失败
 */
int clk_set_rate(int clk_id, uint32_t rate)
{
    if (clk_id >= 0 && clk_id < CLK_COUNT) {
        return dtm_clks[clk_id].ops->set_rate(&dtm_clks[clk_id], rate);
    }
    return -1;
}

#endif /* DTM_BINFDING_CLK_ARM_M7_ST */
