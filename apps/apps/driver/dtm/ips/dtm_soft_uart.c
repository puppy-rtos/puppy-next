#include <stdint.h>
#include <stddef.h>
#include "dtm_bindings.h"
#include "common/dtm_soft_uart.h"
#include "common/dtm_gpio.h"
#include "common/dtm_delay.h"

/**
 * @brief       初始化软件UART
 * @param       uart: 软件UART配置结构体指针
 * @param       tx_port: 发送引脚端口
 * @param       tx_pin: 发送引脚号
 * @param       baudrate: 波特率
 * @retval      0: 初始化成功
 *              -1: 初始化失败
 */
int dtm_soft_uart_init(DTM_SOFT_UART_Type *uart, uint32_t tx_port, uint16_t tx_pin, uint32_t baudrate)
{
    if (uart == NULL) {
        return -1;
    }
    
    /* 配置发送引脚为输出模式 */
    dtm_gpio_set(tx_port, tx_pin, DTM_GPIO_MODE_OUT, DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
    
    /* 设置初始状态为高电平（空闲状态） */
    dtm_gpio_pin_set(tx_port, tx_pin, 1);
    
    /* 计算每比特的延时时间 */
    if (baudrate > 0) {
        uart->delay_us = 1000000 / baudrate;
    } else {
        /* 默认波特率9600 */
        uart->delay_us = 104;
        baudrate = 9600;
    }
    
    /* 保存配置 */
    uart->tx_port = tx_port;
    uart->tx_pin = tx_pin;
    uart->baudrate = baudrate;
    
    /* 等待引脚稳定 */
    dtm_delay_us(uart->delay_us);
    
    return 0;
}

/**
 * @brief       发送一个字节
 * @param       uart: 软件UART配置结构体指针
 * @param       data: 要发送的数据(8位)
 * @retval      None
 */
void dtm_soft_uart_write_byte(DTM_SOFT_UART_Type *uart, uint8_t data)
{
    uint8_t i = 0;
    uint8_t mask = 0x01;
    
    if (uart == NULL) {
        return;
    }
    
    /* 发送起始位（低电平） */
    dtm_gpio_pin_set(uart->tx_port, uart->tx_pin, 0);
    dtm_delay_us(uart->delay_us);
    
    /* 发送8位数据位（从最低位开始） */
    for (i = 0; i < 8; i++) {
        if ((data & mask) != 0) {
            /* 发送高电平 */
            dtm_gpio_pin_set(uart->tx_port, uart->tx_pin, 1);
        } else {
            /* 发送低电平 */
            dtm_gpio_pin_set(uart->tx_port, uart->tx_pin, 0);
        }
        
        dtm_delay_us(uart->delay_us);
        mask <<= 1;
    }
    
    /* 发送停止位（高电平） */
    dtm_gpio_pin_set(uart->tx_port, uart->tx_pin, 1);
    dtm_delay_us(uart->delay_us);
}

/**
 * @brief       发送字符串
 * @param       uart: 软件UART配置结构体指针
 * @param       str: 要发送的字符串
 * @retval      None
 */
void dtm_soft_uart_write_str(DTM_SOFT_UART_Type *uart, const char *str)
{
    if (uart == NULL || str == NULL) {
        return;
    }
    
    /* 逐个字符发送 */
    while (*str != '\0') {
        dtm_soft_uart_write_byte(uart, (uint8_t)*str);
        str++;
    }
}
