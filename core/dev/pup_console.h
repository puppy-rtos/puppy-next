#ifndef __PUP_CONSOLE_H__
#define __PUP_CONSOLE_H__

/**
 * pup_console - unified kernel standard I/O abstraction
 *
 * Provides a single write / read interface for all kernel and user-mode I/O.
 * The default backend is the DTM debug UART with interrupt-driven RX
 * (ring buffer + semaphore) and polling TX.
 *
 * Usage:
 *   pup_console_init()  - called once during board init (replaces syscall_uart_rx_init)
 *   pup_console_write() - output, used by PUP_PRINTK / libcm_console_output
 *   pup_console_read()  - blocking input, used by kernel shell and SYS_READ syscall
 *   pup_console_set_backend() - optional: replace the backend (e.g., USB CDC)
 */

typedef struct {
    int (*write)(const char *buf, int len);
    int (*read)(char *buf, int len);
} pup_console_backend_t;

void pup_console_init(void);
void pup_console_set_backend(const pup_console_backend_t *backend);

int  pup_console_write(const char *buf, int len);
int  pup_console_read(char *buf, int len);
void pup_console_putchar(char c);
int  pup_console_getchar(void);

#endif /* __PUP_CONSOLE_H__ */
