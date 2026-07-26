#include "dtm_bindings.h"

#ifdef DTM_BINFDING_CLK_ARM_M7_GD

#include <stdint.h>
#include <stddef.h>
#include "../common/dtm_clk.h"
#include "../common/dtm_uart.h"
#include "../common/dtm_board.h"
#include "../common/dtm_of_driver.h"

#ifndef DTM_CLK_BASE
#define DTM_CLK_BASE 0x58024400UL
#endif

static uint32_t g_rcu_base = DTM_CLK_BASE;

#define RCU g_rcu_base
/* RCU寄存器定义 */
#define REG32(addr)                  (*(volatile uint32_t *)(uint32_t)(addr))
#define RCU_CTL                         REG32(RCU + 0x00000000U)                    /*!< control register */
#define RCU_PLL0                        REG32(RCU + 0x00000004U)                    /*!< PLL0 register */
#define RCU_CFG0                        REG32(RCU + 0x00000008U)                    /*!< clock configuration register 0 */
#define RCU_INT                         REG32(RCU + 0x0000000CU)                    /*!< clock interrupt register */
#define RCU_AHB1RST                     REG32(RCU + 0x00000010U)                    /*!< AHB1 reset register */
#define RCU_AHB2RST                     REG32(RCU + 0x00000014U)                    /*!< AHB2 reset register */
#define RCU_AHB3RST                     REG32(RCU + 0x00000018U)                    /*!< AHB3 reset register */
#define RCU_AHB4RST                     REG32(RCU + 0x0000001CU)                    /*!< AHB4 reset register */
#define RCU_APB1RST                     REG32(RCU + 0x00000020U)                    /*!< APB1 reset register */
#define RCU_APB2RST                     REG32(RCU + 0x00000024U)                    /*!< APB2 reset register */
#define RCU_APB3RST                     REG32(RCU + 0x00000028U)                    /*!< APB3 reset register */
#define RCU_APB4RST                     REG32(RCU + 0x0000002CU)                    /*!< APB4 reset register */
#define RCU_AHB1EN                      REG32(RCU + 0x00000030U)                    /*!< AHB1 enable register */
#define RCU_AHB2EN                      REG32(RCU + 0x00000034U)                    /*!< AHB2 enable register */
#define RCU_AHB3EN                      REG32(RCU + 0x00000038U)                    /*!< AHB3 enable register */
#define RCU_AHB4EN                      REG32(RCU + 0x0000003CU)                    /*!< AHB4 enable register */
#define RCU_APB1EN                      REG32(RCU + 0x00000040U)                    /*!< APB1 enable register */
#define RCU_APB2EN                      REG32(RCU + 0x00000044U)                    /*!< APB2 enable register */
#define RCU_APB3EN                      REG32(RCU + 0x00000048U)                    /*!< APB3 enable register */
#define RCU_APB4EN                      REG32(RCU + 0x0000004CU)                    /*!< APB4 enable register */
#define RCU_AHB1SPEN                    REG32(RCU + 0x00000050U)                    /*!< AHB1 sleep mode enable register */
#define RCU_AHB2SPEN                    REG32(RCU + 0x00000054U)                    /*!< AHB2 sleep mode enable register */
#define RCU_AHB3SPEN                    REG32(RCU + 0x00000058U)                    /*!< AHB3 sleep mode enable register */
#define RCU_AHB4SPEN                    REG32(RCU + 0x0000005CU)                    /*!< AHB4 sleep mode enable register */
#define RCU_APB1SPEN                    REG32(RCU + 0x00000060U)                    /*!< APB1 sleep mode enable register */
#define RCU_APB2SPEN                    REG32(RCU + 0x00000064U)                    /*!< APB2 sleep mode enable register */
#define RCU_APB3SPEN                    REG32(RCU + 0x00000068U)                    /*!< APB3 sleep mode enable register */
#define RCU_APB4SPEN                    REG32(RCU + 0x0000006CU)                    /*!< APB4 sleep mode enable register */
#define RCU_BDCTL                       REG32(RCU + 0x00000070U)                    /*!< backup domain control register */
#define RCU_RSTSCK                      REG32(RCU + 0x00000074U)                    /*!< reset source / clock register */
#define RCU_PLLADDCTL                   REG32(RCU + 0x00000080U)                    /*!< PLL clock additional control register */
#define RCU_PLL1                        REG32(RCU + 0x00000084U)                    /*!< PLL1 register */
#define RCU_PLL2                        REG32(RCU + 0x00000088U)                    /*!< PLL2 register */
#define RCU_CFG1                        REG32(RCU + 0x0000008CU)                    /*!< clock configuration register 1 */
#define RCU_CFG2                        REG32(RCU + 0x00000090U)                    /*!< clock configuration register 2 */
#define RCU_CFG3                        REG32(RCU + 0x00000094U)                    /*!< clock configuration register 3 */
#define RCU_PLLALL                      REG32(RCU + 0x00000098U)                    /*!< PLL configuration register */
#define RCU_PLL0FRA                     REG32(RCU + 0x0000009CU)                    /*!< PLL0 fraction configuration register */
#define RCU_PLL1FRA                     REG32(RCU + 0x000000A0U)                    /*!< PLL1 fraction configuration register */
#define RCU_PLL2FRA                     REG32(RCU + 0x000000A4U)                    /*!< PLL2 fraction configuration register */
#define RCU_ADDCTL0                     REG32(RCU + 0x000000C0U)                    /*!< additional clock control register 0 */
#define RCU_ADDCTL1                     REG32(RCU + 0x000000C4U)                    /*!< additional clock control register 1 */
#define RCU_ADDINT                      REG32(RCU + 0x000000CCU)                    /*!< additional clock interrupt register */
#define RCU_CFG4                        REG32(RCU + 0x000000D0U)                    /*!< clock configuration register 4  */
#define RCU_USBCLKCTL                   REG32(RCU + 0x000000D4U)                    /*!< USB clock control register */
#define RCU_PLLUSBCFG                   REG32(RCU + 0x000000D8U)                    /*!< PLLUSB configuration register */
#define RCU_ADDAPB2RST                  REG32(RCU + 0x000000E0U)                    /*!< APB2 additional reset register */
#define RCU_ADDAPB2EN                   REG32(RCU + 0x000000E4U)                    /*!< APB2 additional enable register */
#define RCU_ADDAPB2SPEN                 REG32(RCU + 0x000000E8U)                    /*!< APB2 additional sleep mode enable register */
#define RCU_CFG5                        REG32(RCU + 0x000000F0U)

/* 寄存器位定义 */
#define RCU_CTL_IRC64MEN    (1 << 30)  /* IRC64M内部64MHz RC振荡器使能 */
#define RCU_CTL_IRC64MSTB   (1 << 31)  /* IRC64M内部64MHz RC振荡器稳定标志 */
#define RCU_CTL_HXTALEN     (1 << 16)  /* 高速外部晶体振荡器使能 */
#define RCU_CTL_HXTALSTB    (1 << 17)  /* 高速外部晶体振荡器稳定标志 */
#define RCU_CTL_PLL0EN      (1 << 24)  /* PLL0使能 */
#define RCU_CTL_PLL0STB     (1 << 25)  /* PLL0稳定标志 */
// SCS[1:0] 系统时钟选择
// 由软件配置选择系统时钟源。由于CK_SYS的改变存在固有的延迟，因此软件应当读
// SCSS位来确保时钟源切换是否结束。在从深度睡眠或待机模式中返回时，以及当
// HXTAL直接或间接作为系统时钟同时HXTAL时钟监视器检测到HXTAL故障时，强制
// 选择CK_IRC64MDIV作为系统时钟。
// 00：选择 CK_IRC64MDIV 时钟作为 CK_SYS 时钟源
// 01：选择 CK_HXTAL 时钟作为 CK_SYS 时钟源
// 10：选择 CK_LPIRC4M 时钟作为 CK_SYS 时钟源
// 11：选择 CK_PLL0P 时钟作为 CK_SYS 时钟源
#define RCU_CFG0_SCS        (3 << 0)   /* 系统时钟源选择 */
#define RCU_CFG0_SCSS       (3 << 2)   /* 系统时钟源切换状态 */
#define RCU_CFG0_SCS_IRC64M (0 << 0)   /* 选择IRC64M作为系统时钟 */
#define RCU_CFG0_SCS_HXTAL  (1 << 0)   /* 选择HXTAL作为系统时钟 */
#define RCU_CFG0_SCS_PLL0   (3 << 0)   /* 选择PLL0作为系统时钟 */

#define RCU_CFG0_AHBPSC     (0xF << 4) /* AHB预分频器选择 */
#define RCU_CFG0_APB1PSC    (0x7 << 10)/* APB1预分频器选择 */
#define RCU_CFG0_APB2PSC    (0x7 << 13)/* APB2预分频器选择 */
#define RCU_CFG0_APB3PSC    (0x7 << 27)/* APB3预分频器选择 */
#define RCU_CFG0_APB4PSC    (0x7 << 24)/* APB4预分频器选择 */

/* 固定频率时钟操作函数 */
static uint32_t fixed_clk_get_rate(const struct clk_desc *clk)
{
    return clk->freq;
}

static int fixed_clk_enable(struct clk_desc *clk)
{
    if (clk == &dtm_clks[CLK_HSI]) {
        RCU_CTL |= RCU_CTL_IRC64MEN;
        while (!(RCU_CTL & RCU_CTL_IRC64MSTB)); /* 等待IRC64M就绪 */
    } else if (clk == &dtm_clks[CLK_HSE]) {
        RCU_CTL |= RCU_CTL_HXTALEN;
        while (!(RCU_CTL & RCU_CTL_HXTALSTB)); /* 等待HXTAL就绪 */
    }
    clk->enabled = 1;
    return 0;
}

static int fixed_clk_disable(struct clk_desc *clk)
{
    if (clk == &dtm_clks[CLK_HSI]) {
        RCU_CTL &= ~RCU_CTL_IRC64MEN;
    } else if (clk == &dtm_clks[CLK_HSE]) {
        RCU_CTL &= ~RCU_CTL_HXTALEN;
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
    dtm_clks[clk_id].enr_reg = (volatile uint32_t *)(uintptr_t)(g_rcu_base + enable_offset);
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

static void dtm_clk_apply_default_register_refs(void)
{
    if (g_rcu_base == 0U) {
        return;
    }

    if (dtm_clks[CLK_GPIOA].enr_reg == NULL) dtm_clks[CLK_GPIOA].enr_reg = &RCU_AHB4EN;
    if (dtm_clks[CLK_GPIOB].enr_reg == NULL) dtm_clks[CLK_GPIOB].enr_reg = &RCU_AHB4EN;
    if (dtm_clks[CLK_GPIOC].enr_reg == NULL) dtm_clks[CLK_GPIOC].enr_reg = &RCU_AHB4EN;
    if (dtm_clks[CLK_GPIOD].enr_reg == NULL) dtm_clks[CLK_GPIOD].enr_reg = &RCU_AHB4EN;
    if (dtm_clks[CLK_GPIOE].enr_reg == NULL) dtm_clks[CLK_GPIOE].enr_reg = &RCU_AHB4EN;
    if (dtm_clks[CLK_GPIOF].enr_reg == NULL) dtm_clks[CLK_GPIOF].enr_reg = &RCU_AHB4EN;
    if (dtm_clks[CLK_GPIOG].enr_reg == NULL) dtm_clks[CLK_GPIOG].enr_reg = &RCU_AHB4EN;
    if (dtm_clks[CLK_GPIOH].enr_reg == NULL) dtm_clks[CLK_GPIOH].enr_reg = &RCU_AHB4EN;
    if (dtm_clks[CLK_GPIOI].enr_reg == NULL) dtm_clks[CLK_GPIOI].enr_reg = &RCU_AHB4EN;

    if (dtm_clks[CLK_UART1].enr_reg == NULL) dtm_clks[CLK_UART1].enr_reg = &RCU_APB2EN;
    if (dtm_clks[CLK_UART2].enr_reg == NULL) dtm_clks[CLK_UART2].enr_reg = &RCU_APB1EN;
    if (dtm_clks[CLK_UART3].enr_reg == NULL) dtm_clks[CLK_UART3].enr_reg = &RCU_APB1EN;
    if (dtm_clks[CLK_UART4].enr_reg == NULL) dtm_clks[CLK_UART4].enr_reg = &RCU_APB1EN;
    if (dtm_clks[CLK_UART5].enr_reg == NULL) dtm_clks[CLK_UART5].enr_reg = &RCU_APB1EN;
    if (dtm_clks[CLK_UART6].enr_reg == NULL) dtm_clks[CLK_UART6].enr_reg = &RCU_APB2EN;
    if (dtm_clks[CLK_UART7].enr_reg == NULL) dtm_clks[CLK_UART7].enr_reg = &RCU_APB1EN;
    if (dtm_clks[CLK_UART8].enr_reg == NULL) dtm_clks[CLK_UART8].enr_reg = &RCU_APB1EN;
}

static int dtm_clk_arm_m7_gd_of_probe(dtm_ofw_node_t node)
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
        g_rcu_base = config.base;
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
    if (dtm_ofw_prop_read_u32_index(node, "puppy,bus-dividers", 3, &value) == 0) {
        config.pclk3_divider = value;
    }
    if (dtm_ofw_prop_read_u32_index(node, "puppy,bus-dividers", 4, &value) == 0) {
        config.pclk4_divider = value;
    }

    dtm_clk_parse_gate_nodes(node);
    dtm_clk_apply_default_register_refs();
    dtm_board_set_clock(&config);
    return 0;
}

static const char *const g_dtm_clk_arm_m7_gd_compatibles[] = {
    "puppy,clk-arm-m7-gd",
    "gd,gd32h7-rcu",
};

const dtm_of_driver_t g_dtm_clk_arm_m7_gd_of_driver = {
    "clk-arm-m7-gd",
    g_dtm_clk_arm_m7_gd_compatibles,
    DTM_ARRAY_SIZE(g_dtm_clk_arm_m7_gd_compatibles),
    dtm_clk_arm_m7_gd_of_probe,
};

/* PLL时钟操作函数 */
static uint32_t pll_clk_get_rate(const struct clk_desc *clk)
{
    // 如果是PLL_VCO、PLL_P或PLL_Q或PLL_R节点，使用CLK_PLL的参数进行计算
    if (clk == &dtm_clks[CLK_PLL_VCO] || clk == &dtm_clks[CLK_PLL_P] || 
        clk == &dtm_clks[CLK_PLL_Q]) {
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
    RCU_CTL |= RCU_CTL_PLL0EN;
    while (!(RCU_CTL & RCU_CTL_PLL0STB)); /* 等待PLL0就绪 */
    clk->enabled = 1;
    return 0;
}

static int pll_clk_disable(struct clk_desc *clk)
{
    RCU_CTL &= ~RCU_CTL_PLL0EN;
    clk->enabled = 0;
    return 0;
}

// 17:16 PLLSEL[1:0] PLLs时钟源选择
// 由软件置位或复位，控制PLLs时钟源
// 00：选择 CK_IRC64MDIV 时钟作为 PLL、PLL1、PLL2 源时钟
// 01：选择 CK_LPIRC4M 时钟作为 PLL、PLL1、PLL2 源时钟
// 10：选择 CK_HXTAL 时钟作为 PLL、PLL1、PLL2 源时钟
// 11：无时钟作为PLL、PLL1、PLL2源时钟
static int pll_clk_set_parent(struct clk_desc* clk, int parent_id)
{
    if (parent_id == CLK_HSI || parent_id == CLK_HSE) {
        clk->parent = parent_id;
        /* 更新PLL0时钟源选择 */
        if (parent_id == CLK_HSI) {
            // 选择IRC64M作为PLL0时钟源
            RCU_PLLALL &= ~(0x3 << 16); /* 清除时钟源选择 */
            RCU_PLLALL |= (0x0 << 16); /* 设置为IRC64M */
        } else {
            // 选择HXTAL作为PLL0时钟源
            RCU_PLLALL &= ~(0x3 << 16); /* 清除时钟源选择 */
            RCU_PLLALL |= (0x2 << 16); /* 设置为HXTAL */

            // PLL0 VCO selection
            RCU_PLLALL |= (0x1 << 2); /* 设置为PLL0_VCO */
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
    uint32_t sysclk_src = (RCU_CFG0 & RCU_CFG0_SCSS) >> 2;
    
    switch (sysclk_src) {
        case RCU_CFG0_SCS_IRC64M: /* IRC64M */
            return dtm_clks[CLK_HSI].ops->get_rate(&dtm_clks[CLK_HSI]);
        case RCU_CFG0_SCS_HXTAL:  /* HXTAL */
            return dtm_clks[CLK_HSE].ops->get_rate(&dtm_clks[CLK_HSE]);
        case RCU_CFG0_SCS_PLL0:   /* PLL0 */
            return dtm_clks[CLK_PLL_P].ops->get_rate(&dtm_clks[CLK_PLL_P]);
        default:
            return 0;
    }
}

static int sysclk_set_parent(struct clk_desc *clk, int parent_id)
{
    uint32_t scs_val;
    
    switch (parent_id) {
        case CLK_HSI:
            scs_val = RCU_CFG0_SCS_IRC64M;
            break;
        case CLK_HSE:
            scs_val = RCU_CFG0_SCS_HXTAL;
            break;
        case CLK_PLL_P:
            scs_val = RCU_CFG0_SCS_PLL0;
            break;
        default:
            return -1;
    }
    
    /* 设置系统时钟源 */
    RCU_CFG0 = (RCU_CFG0 & ~RCU_CFG0_SCS) | scs_val;
    
    /* 等待系统时钟源切换完成 */
    while (((RCU_CFG0 & RCU_CFG0_SCSS) >> 2) != scs_val);
    
    return 0;
}

static int sysclk_enable(struct clk_desc *clk)
{
    clk->enabled = 1;
    return 0;
}

static int sysclk_disable(struct clk_desc *clk)
{
    clk->enabled = 0;
    return 0;
}

static const struct clk_ops sysclk_ops = {
    .get_rate = sysclk_get_rate,
    .enable = sysclk_enable,
    .disable = sysclk_disable,
    .set_parent = sysclk_set_parent,
    .set_rate = NULL, /* 系统时钟频率由PLL参数决定，不直接设置 */
};

/* 外设时钟操作函数 */
static uint32_t peri_clk_get_rate(const struct clk_desc *clk)
{
    const struct clk_desc *parent = &dtm_clks[clk->parent];
    return parent->ops->get_rate(parent);
}

static int peri_clk_enable(struct clk_desc *clk)
{
    if (clk->enr_reg) {
        (*(volatile uint32_t *)clk->enr_reg) |= clk->enr_bit;
        clk->enabled = 1;
        return 0;
    }
    return -1;
}

static int peri_clk_disable(struct clk_desc *clk)
{
    if (clk->enr_reg) {
        (*(volatile uint32_t *)clk->enr_reg) &= ~clk->enr_bit;
        clk->enabled = 0;
        return 0;
    }
    return -1;
}

static const struct clk_ops peri_clk_ops = {
    .get_rate = peri_clk_get_rate,
    .enable = peri_clk_enable,
    .disable = peri_clk_disable,
    .set_parent = NULL,
    .set_rate = NULL,
};

/* 时钟描述符数组 */
struct clk_desc dtm_clks[CLK_COUNT] = {
    
    /* 固定频率时钟 */
    [CLK_HSI] = {.name = "hsi", .parent = -1, .ops = &fixed_clk_ops, .freq = 64000000, .enabled = 0},
    [CLK_HSE] = {.name = "hse", .parent = -1, .ops = &fixed_clk_ops, .freq = DTM_HSE_FREQ, .enabled = 0},
    
    /* PLL时钟 */
    [CLK_PLL] = {.name = "pll", .parent = CLK_HSE, .ops = &pll_clk_ops, .plln = 40, .pllm = 5, .pllp = 1, .pllq = 2, .pllr = 2, .enabled = 0},
    [CLK_PLL_VCO] = {.name = "pll_vco", .parent = CLK_PLL, .ops = &pll_clk_ops, .enabled = 0},
    [CLK_PLL_P] = {.name = "pll_p", .parent = CLK_PLL, .ops = &pll_clk_ops, .enabled = 0},
    [CLK_PLL_Q] = {.name = "pll_q", .parent = CLK_PLL, .ops = &pll_clk_ops, .enabled = 0},

    
    /* 系统时钟 */
    [CLK_SYSCLK] = {.name = "sysclk", .parent = CLK_PLL_P, .ops = &sysclk_ops, .enabled = 0},
    [CLK_HCLK] = {.name = "hclk", .parent = CLK_SYSCLK, .ops = &div_clk_ops, .div = 1, .enabled = 0},
    [CLK_PCLK1] = {.name = "pclk1", .parent = CLK_HCLK, .ops = &div_clk_ops, .div = 2, .enabled = 0},
    [CLK_PCLK2] = {.name = "pclk2", .parent = CLK_HCLK, .ops = &div_clk_ops, .div = 1, .enabled = 0},
    [CLK_PCLK3] = {.name = "pclk3", .parent = CLK_HCLK, .ops = &div_clk_ops, .div = 2, .enabled = 0},
    [CLK_PCLK4] = {.name = "pclk4", .parent = CLK_HCLK, .ops = &div_clk_ops, .div = 2, .enabled = 0},
    
    /* 外设时钟 */
    [CLK_GPIOA] = {.name = "gpioa", .parent = CLK_HCLK, .ops = &peri_clk_ops, .enr_bit = (1 << 0), .enabled = 0},
    [CLK_GPIOB] = {.name = "gpiob", .parent = CLK_HCLK, .ops = &peri_clk_ops, .enr_bit = (1 << 1), .enabled = 0},
    [CLK_GPIOC] = {.name = "gpioc", .parent = CLK_HCLK, .ops = &peri_clk_ops, .enr_bit = (1 << 2), .enabled = 0},
    [CLK_GPIOD] = {.name = "gpiod", .parent = CLK_HCLK, .ops = &peri_clk_ops, .enr_bit = (1 << 3), .enabled = 0},
    [CLK_GPIOE] = {.name = "gpioe", .parent = CLK_HCLK, .ops = &peri_clk_ops, .enr_bit = (1 << 4), .enabled = 0},
    [CLK_GPIOF] = {.name = "gpiof", .parent = CLK_HCLK, .ops = &peri_clk_ops, .enr_bit = (1 << 5), .enabled = 0},
    [CLK_GPIOG] = {.name = "gpiog", .parent = CLK_HCLK, .ops = &peri_clk_ops, .enr_bit = (1 << 6), .enabled = 0},
    [CLK_GPIOH] = {.name = "gpioh", .parent = CLK_HCLK, .ops = &peri_clk_ops, .enr_bit = (1 << 7), .enabled = 0},
    [CLK_GPIOI] = {.name = "gpioi", .parent = CLK_HCLK, .ops = &peri_clk_ops, .enr_bit = (1 << 8), .enabled = 0},


    
    [CLK_UART1] = {.name = "uart1", .parent = CLK_PCLK2, .ops = &peri_clk_ops, .enr_bit = (1 << 4), .enabled = 0},
    [CLK_UART2] = {.name = "uart2", .parent = CLK_PCLK1, .ops = &peri_clk_ops, .enr_bit = (1 << 17), .enabled = 0},
    [CLK_UART3] = {.name = "uart3", .parent = CLK_PCLK1, .ops = &peri_clk_ops, .enr_bit = (1 << 18), .enabled = 0},
    [CLK_UART4] = {.name = "uart4", .parent = CLK_PCLK1, .ops = &peri_clk_ops, .enr_bit = (1 << 19), .enabled = 0},
    [CLK_UART5] = {.name = "uart5", .parent = CLK_PCLK1, .ops = &peri_clk_ops, .enr_bit = (1 << 20), .enabled = 0},
    [CLK_UART6] = {.name = "uart6", .parent = CLK_PCLK2, .ops = &peri_clk_ops, .enr_bit = (1 << 5), .enabled = 0},
    [CLK_UART7] = {.name = "uart7", .parent = CLK_PCLK1, .ops = &peri_clk_ops, .enr_bit = (1 << 30), .enabled = 0},
    [CLK_UART8] = {.name = "uart8", .parent = CLK_PCLK1, .ops = &peri_clk_ops, .enr_bit = (1 << 31), .enabled = 0}
};
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
/* 时钟初始化函数 */
void dtm_clk_init(void)
{
    pup_dtm_clock_config_t board_clock;

    if (dtm_board_get_clock(&board_clock) == 0) {
        if (board_clock.base != 0U) {
            g_rcu_base = board_clock.base;
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
        if (board_clock.pclk3_divider != 0U) {
            dtm_clks[CLK_PCLK3].div = board_clock.pclk3_divider;
        }
        if (board_clock.pclk4_divider != 0U) {
            dtm_clks[CLK_PCLK4].div = board_clock.pclk4_divider;
        }
    }

    /* 使能HSE */
    dtm_clk_apply_default_register_refs();
    clk_enable(CLK_HSE);
    
    /* 配置PLL */
    clk_set_parent(CLK_PLL, CLK_HSE);
    
    /* 设置PLL参数 */
    const struct clk_desc *pll_clk = &dtm_clks[CLK_PLL];
    
    /* 配置PLL0M[5:0]: 输入分频系数 */
    RCU_PLL0 &= ~(0x3F << 0);
    RCU_PLL0 |= (pll_clk->pllm) << 0;  /* 寄存器值是实际值 */
    
    /* 配置PLL0N[14:6]: 倍频系数 */
    // PLL0N 的值必须满足：
    // 9≤PLL0N≤512
    RCU_PLL0 &= ~(0x1FFUL << 6);
    RCU_PLL0 |= (pll_clk->plln - 1) << 6;  /* 寄存器值是实际值减1 */
    
    /* 配置PLL0P[22:16]: P输出分频系数 */
    RCU_PLL0 &= ~(0x7FUL << 16);
    RCU_PLL0 |= (pll_clk->pllp - 1) << 16; /* 寄存器值是实际值减1 */
    
    /* 配置PLL0R[30:24]: R输出分频系数 */
    RCU_PLL0 &= ~(0x7FUL << 24);
    RCU_PLL0 |= (pll_clk->pllr - 1) << 24; /* 寄存器值是实际值减1 */
    
    /* RCU_PLLADDCTL 配置PLL0Q[6:0]: Q输出分频系数 */
    RCU_PLLADDCTL &= ~(0x7FUL << 0);
    RCU_PLLADDCTL |= (pll_clk->pllq - 1) << 0; /* 寄存器值是实际值减1 */
    // 25 PLL0PEN PLL0P 分频器输出使能
    // 23 PLL0REN PLL0Q 分频器输出使能
    // 24 PLL0REN PLL0R 分频器输出使能
    RCU_PLLADDCTL |= (1 << 25) | (1 << 23) | (1 << 24);

    // 使能HCLK、PCLK1、PCLK2
    //     7:4 AHBPSC[3:0] AHB / AXI 预分频选择
    RCU_CFG0 &= ~(0x0FUL << 4);
    uint32_t hclk_div = dtm_clks[CLK_HCLK].div;
    RCU_CFG0 |= (get_ahb_prescaler_bits(hclk_div) << 4); // 选择 CK_SYS 时钟 256 分频
    /* 配置APB1/2/3/4分频 */
    uint32_t pclk1_div = dtm_clks[CLK_PCLK1].div;
    uint32_t pclk2_div = dtm_clks[CLK_PCLK2].div;
    uint32_t pclk3_div = dtm_clks[CLK_PCLK3].div;
    uint32_t pclk4_div = dtm_clks[CLK_PCLK4].div;
//     12:10 APB1PSC[2:0] APB1 预分频选择
    RCU_CFG0 &= ~(0x07UL << 10);
    RCU_CFG0 |= (get_apb_prescaler_bits(pclk1_div) << 10); // 选择 CK_AHB 时钟 2 分频
//     15:13 APB2PSC[2:0] APB2 预分频选择
    RCU_CFG0 &= ~(0x07UL << 13);
    RCU_CFG0 |= (get_apb_prescaler_bits(pclk2_div) << 13); // 选择 CK_AHB 时钟 2 分频
//     29:27 APB3PSC[2:0] APB3 预分频选择
    RCU_CFG0 &= ~(0x07UL << 27);
    RCU_CFG0 |= (get_apb_prescaler_bits(pclk3_div) << 27); // 选择 CK_AHB 时钟 2 分频
// 26:24 APB4PSC[2:0] APB4 预分频选择
    RCU_CFG0 &= ~(0x07UL << 24);
    RCU_CFG0 |= (get_apb_prescaler_bits(pclk4_div) << 24); // 选择 CK_AHB 时钟 2 分频

    //  configure PLL0 input and output range
    RCU_PLLALL &= ~(0x3 << 0); /* 清除输入范围选择 */
    RCU_PLLALL |= (0x2 << 0); /* 设置为4MHz~8MHz */

    /* 使能PLL */
    clk_enable(CLK_PLL);
    
    /* 设置PLL_P为系统时钟 */
    clk_set_parent(CLK_SYSCLK, CLK_PLL_P);

}

/**
 * @brief       dump the clock tree by printing the current clock configuration
 * @note        This function prints the current clock configuration in a tree format.
 * @retval      None
 */
void dtm_clk_dumptree(void)
{
    uint32_t hse = DTM_HSE_FREQ;
    uint32_t hsi = 64000000; // GD32H7 HSI 默认64MHz
    
    /* 从RCU寄存器读取PLL参数 */
    uint32_t pllm = (RCU_PLL0 >> 0) & 0x3F;       // PLL0M[5:0] in RCU_PLL0
    uint32_t plln = ((RCU_PLL0 >> 6) & 0x1FFUL) + 1;  // PLL0N[14:6] in RCU_PLL0, +1是因为寄存器值是实际值减1
    uint32_t pllp = ((RCU_PLL0 >> 16) & 0x3F) + 1; // PLL0P[22:16] in RCU_PLL0, +1是因为寄存器值是实际值减1
    uint32_t pllq = ((RCU_PLLADDCTL >> 0) & 0x3FUL) + 1; // PLL0Q[6:0] in RCU_PLLADDCTL, +1是因为寄存器值是实际值减1
    
    /* 获取系统时钟源 */
    uint32_t sysclk_src = (RCU_CFG0 >> 2) & 0x03;  // SCS[1:0] in RCU_CFG0
    
    /* 计算PLL VCO频率 */
    uint32_t pll_vco = (uint64_t)hse * plln / pllm;
    uint32_t pll_p_ck = pll_vco / pllp;
    uint32_t pll_q_ck = pll_vco / pllq;
    
    /* 计算系统时钟频率 */
    uint32_t sysclk = 0;
    if (sysclk_src == RCU_CFG0_SCS_IRC64M)
        sysclk = hsi; // HSI
    else if (sysclk_src == RCU_CFG0_SCS_HXTAL)
        sysclk = hse; // HSE
    else if (sysclk_src == RCU_CFG0_SCS_PLL0)
        sysclk = pll_p_ck; // PLL
    else
        sysclk = 0;
    
    /* 获取AHB和APB分频 */
    uint32_t ahb_div = (RCU_CFG0 >> 4) & 0x0F;     // HPRE[3:0] in RCU_CFG0
    uint32_t apb1_div = (RCU_CFG0 >> 10) & 0x07;    // PPRE1[2:0] in RCU_CFG0
    uint32_t apb2_div = (RCU_CFG0 >> 13) & 0x07;   // PPRE2[2:0] in RCU_CFG0
    
    /* 分频系数表 */
    const uint16_t ahb_presc_tbl[16] = {1, 1, 1, 1, 1, 1, 1, 1, 2, 4, 8, 16, 64, 128, 256, 512};
    const uint8_t apb_presc_tbl[8] = {1, 1, 1, 1, 2, 4, 8, 16};
    
    uint32_t hclk = sysclk / ahb_presc_tbl[ahb_div];
    uint32_t pclk1 = hclk / apb_presc_tbl[apb1_div];
    uint32_t pclk2 = hclk / apb_presc_tbl[apb2_div];
    
    dtm_printf("clock-tree\n");
    dtm_printf("├── HSE (%lu Hz)\n", hse);
    dtm_printf("│   └── PLL\n");
    dtm_printf("│       ├── VCO (%lu Hz)\n", pll_vco);
    dtm_printf("│       ├── PLL_P (SYSCLK, %lu Hz)\n", pll_p_ck);
    dtm_printf("│       │   └── AHB (HCLK, %lu Hz)\n", hclk);
    dtm_printf("│       │       ├── APB1 (PCLK1, %lu Hz)\n", pclk1);
    dtm_printf("│       │       └── APB2 (PCLK2, %lu Hz)\n", pclk2);
    dtm_printf("│       └── PLL_Q (USB, %lu Hz)\n", pll_q_ck);
    dtm_printf("├── HSI (%lu Hz)\n", hsi);
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
        dtm_clk_apply_default_register_refs();
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
        dtm_clk_apply_default_register_refs();
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

#endif /* DTM_BINFDING_CLK_ARM_M7_GD */
