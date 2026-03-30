#include <stdint.h>

/* 软件UART配置结构体 */
typedef struct {
    uint32_t tx_port;     /*!< 发送引脚端口 */
    uint16_t tx_pin;      /*!< 发送引脚号 */
    uint32_t baudrate;    /*!< 波特率 */
    uint32_t delay_us;    /*!< 每比特的延时时间(微秒) */
} DTM_SOFT_UART_Type;

/**
 * @brief       初始化软件UART
 * @param       uart: 软件UART配置结构体指针
 * @param       tx_port: 发送引脚端口
 * @param       tx_pin: 发送引脚号
 * @param       baudrate: 波特率
 * @retval      0: 初始化成功
 *              -1: 初始化失败
 */
int dtm_soft_uart_init(DTM_SOFT_UART_Type *uart, uint32_t tx_port, uint16_t tx_pin, uint32_t baudrate);

/**
 * @brief       发送一个字节
 * @param       uart: 软件UART配置结构体指针
 * @param       data: 要发送的数据(8位)
 * @retval      None
 */
void dtm_soft_uart_write_byte(DTM_SOFT_UART_Type *uart, uint8_t data);

/**
 * @brief       发送字符串
 * @param       uart: 软件UART配置结构体指针
 * @param       str: 要发送的字符串
 * @retval      None
 */
void dtm_soft_uart_write_str(DTM_SOFT_UART_Type *uart, const char *str);
