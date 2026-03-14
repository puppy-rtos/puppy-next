#include "pup_console.h"
#include <puppy_core.h>
#include "dtm_uart.h"
#include "dtm_bindings.h"
#include "arch/arm/cortex-m4/dtm_irq.h"

/* ---- RX ring buffer ----------------------------------------------------- */

#define CONSOLE_RX_BUF_SIZE 128

static sem_t               g_rx_sem;
static volatile uint8_t    g_rx_buf[CONSOLE_RX_BUF_SIZE];
static volatile unsigned int g_rx_head;
static volatile unsigned int g_rx_tail;
static volatile int         g_inited;

static const pup_console_backend_t *g_backend;

/* ---- default DTM UART backend ------------------------------------------- */

static int uart_default_write(const char *buf, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        if (buf[i] == '\n')
            dtm_uart_putc('\r');
        dtm_uart_putc(buf[i]);
    }
    return len;
}

static int uart_default_read(char *buf, int len)
{
    int n = 0;
    uint8_t ch;
    pup_base_t key;

    while (n < len) {
        key = arch_irq_lock();
        if (g_rx_tail != g_rx_head) {
            ch = g_rx_buf[g_rx_tail];
            g_rx_tail = (g_rx_tail + 1U) % CONSOLE_RX_BUF_SIZE;
            arch_irq_unlock(key);
            buf[n++] = (char)ch;
            continue;
        }
        arch_irq_unlock(key);
        sem_wait(&g_rx_sem);
    }
    return n;
}

static const pup_console_backend_t g_uart_backend = {
    .write = uart_default_write,
    .read  = uart_default_read,
};

static void uart_rx_irq_cb(uint8_t data)
{
    unsigned int next = (g_rx_head + 1U) % CONSOLE_RX_BUF_SIZE;

    if (next != g_rx_tail) {
        g_rx_buf[g_rx_head] = data;
        g_rx_head = next;
        sem_post(&g_rx_sem);
    }
}

/* ---- public API ---------------------------------------------------------- */

void pup_console_init(void)
{
    pup_base_t key = arch_irq_lock();

    if (g_inited) {
        arch_irq_unlock(key);
        return;
    }

    g_rx_head = 0;
    g_rx_tail = 0;
    sem_init(&g_rx_sem, 0, 0);
    g_backend = &g_uart_backend;

    dtm_uart_set_rx_callback(uart_rx_irq_cb);
    dtm_uart_enable_rx_it();
    dtm_nvic_enable_irq(DTM_ChipSpecific_IRQn + DTM_DEBUG_UART_IRQn);
    dtm_nvic_set_priority(DTM_ChipSpecific_IRQn + DTM_DEBUG_UART_IRQn, 1);

    g_inited = 1;
    arch_irq_unlock(key);
}

void pup_console_set_backend(const pup_console_backend_t *backend)
{
    g_backend = backend ? backend : &g_uart_backend;
}

int pup_console_write(const char *buf, int len)
{
    if (!buf || len <= 0 || !g_backend || !g_backend->write)
        return 0;
    return g_backend->write(buf, len);
}

int pup_console_read(char *buf, int len)
{
    if (!buf || len <= 0 || !g_backend || !g_backend->read)
        return -1;
    return g_backend->read(buf, len);
}

void pup_console_putchar(char c)
{
    pup_console_write(&c, 1);
}

int pup_console_getchar(void)
{
    char c;
    int r = pup_console_read(&c, 1);
    return (r > 0) ? (int)(unsigned char)c : -1;
}
