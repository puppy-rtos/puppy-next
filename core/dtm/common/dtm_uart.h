#include "stdint.h"

/* UART回调函数类型 */
typedef void (*dtm_uart_rx_callback_t)(uint8_t data);

int dtm_uart_putc(char ch);
void dtm_uart_init(uint32_t baudrate);

/**
 * @brief       设置UART接收中断回调函数
 * @param       callback: 回调函数指针
 * @retval      None
 */
void dtm_uart_set_rx_callback(dtm_uart_rx_callback_t callback);

/**
 * @brief       使能UART接收中断
 * @retval      None
 */
void dtm_uart_enable_rx_it(void);

/**
 * @brief       禁用UART接收中断
 * @retval      None
 */
void dtm_uart_disable_rx_it(void);

/**
 * @brief       获取接收到的字符
 * @retval      接收到的字符
 */
uint8_t dtm_uart_getc(void);

#ifdef __GNUC__
#include <stdarg.h>
int dtm_printf(const char *fmt, ...);
#endif
