#include "dtm_board.h"

#include "dtm_bindings.h"
#include "dtm_gpio.h"
#include "dtm_of_driver.h"
#include "dtm_ofw.h"
#include "libcm.h"

#ifndef DTM_DEBUG_UART
#define DTM_DEBUG_UART 0U
#endif

#ifndef DTM_DEBUG_UART_IRQn
#define DTM_DEBUG_UART_IRQn 0U
#endif

#ifndef DTM_HSE_FREQ
#define DTM_HSE_FREQ 0U
#endif

#ifndef DTM_SYSCLK_FREQ
#define DTM_SYSCLK_FREQ 0U
#endif

#ifndef DTM_CLK_BASE
#define DTM_CLK_BASE 0U
#endif

#ifndef DTM_PLLM
#define DTM_PLLM 8U
#endif

#ifndef DTM_PLLN
#define DTM_PLLN 384U
#endif

#ifndef DTM_PLLP
#define DTM_PLLP 4U
#endif

#ifndef DTM_PLLQ
#define DTM_PLLQ 8U
#endif

#ifndef DTM_HCLK_DIVIDER
#define DTM_HCLK_DIVIDER 0U
#endif

#ifndef DTM_PCLK1_DIVIDER
#define DTM_PCLK1_DIVIDER 0U
#endif

#ifndef DTM_PCLK2_DIVIDER
#define DTM_PCLK2_DIVIDER 0U
#endif

#ifndef DTM_PCLK3_DIVIDER
#define DTM_PCLK3_DIVIDER 0U
#endif

#ifndef DTM_PCLK4_DIVIDER
#define DTM_PCLK4_DIVIDER 0U
#endif

static pup_dtm_uart_config_t g_board_console;
static pup_dtm_clock_config_t g_board_clock;
static pup_dtm_exti_config_t g_board_key_exti;

#ifdef DTM_BINFDING_CLK_ARM_M4_ST_GD
extern const dtm_of_driver_t g_dtm_clk_arm_m4_st_gd_of_driver;
#endif
#ifdef DTM_BINFDING_CLK_ARM_M7_ST
extern const dtm_of_driver_t g_dtm_clk_arm_m7_st_of_driver;
#endif
#ifdef DTM_BINFDING_CLK_ARM_M7_GD
extern const dtm_of_driver_t g_dtm_clk_arm_m7_gd_of_driver;
#endif
#ifdef DTM_BINFDING_UART_ARM_M4_ST_GD
extern const dtm_of_driver_t g_dtm_uart_arm_m4_st_gd_of_driver;
#endif
#ifdef DTM_BINFDING_UART_ARM_M7_ST
extern const dtm_of_driver_t g_dtm_uart_arm_m7_st_of_driver;
#endif
extern const dtm_of_driver_t g_dev_irq_exti_controller_of_driver;
extern const dtm_of_driver_t g_dev_irq_syscfg_of_driver;
extern const dtm_of_driver_t g_dev_irq_gpio_key_of_driver;

int dtm_board_init(void)
{
    const dtm_of_driver_t *drivers[] = {
#ifdef DTM_BINFDING_CLK_ARM_M4_ST_GD
        &g_dtm_clk_arm_m4_st_gd_of_driver,
#endif
#ifdef DTM_BINFDING_CLK_ARM_M7_ST
        &g_dtm_clk_arm_m7_st_of_driver,
#endif
#ifdef DTM_BINFDING_CLK_ARM_M7_GD
        &g_dtm_clk_arm_m7_gd_of_driver,
#endif
#ifdef DTM_BINFDING_UART_ARM_M4_ST_GD
        &g_dtm_uart_arm_m4_st_gd_of_driver,
#endif
#ifdef DTM_BINFDING_UART_ARM_M7_ST
        &g_dtm_uart_arm_m7_st_of_driver,
#endif
        &g_dev_irq_syscfg_of_driver,
        &g_dev_irq_exti_controller_of_driver,
        &g_dev_irq_gpio_key_of_driver,
    };

    if (dtm_ofw_is_ready()) {
        dtm_of_probe_all(drivers, DTM_ARRAY_SIZE(drivers));
    }

    return 0;
}

void dtm_board_set_console(const pup_dtm_uart_config_t *config)
{
    if (config) {
        libcm_memcpy(&g_board_console, config, sizeof(g_board_console));
    }
}

void dtm_board_set_clock(const pup_dtm_clock_config_t *config)
{
    if (config) {
        libcm_memcpy(&g_board_clock, config, sizeof(g_board_clock));
    }
}

void dtm_board_set_key_exti(const pup_dtm_exti_config_t *config)
{
    if (config) {
        libcm_memcpy(&g_board_key_exti, config, sizeof(g_board_key_exti));
    }
}

int dtm_board_get_console(pup_dtm_uart_config_t *out_config)
{
    if (!out_config) {
        return -1;
    }

    libcm_memcpy(out_config, &g_board_console, sizeof(*out_config));
    return g_board_console.valid ? 0 : -1;
}

int dtm_board_get_clock(pup_dtm_clock_config_t *out_config)
{
    if (!out_config) {
        return -1;
    }

    libcm_memcpy(out_config, &g_board_clock, sizeof(*out_config));
    return 0;
}

int dtm_board_get_key_exti(pup_dtm_exti_config_t *out_config)
{
    if (!out_config) {
        return -1;
    }

    libcm_memcpy(out_config, &g_board_key_exti, sizeof(*out_config));
    return g_board_key_exti.valid ? 0 : -1;
}
