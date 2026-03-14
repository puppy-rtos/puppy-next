#ifndef __LIBCM_H__
#define __LIBCM_H__

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define LIBCM_PRINTK_BUF_SIZE 256

#define LIBCM_ZEROPAD     (1 << 0)
#define LIBCM_SIGN        (1 << 1)
#define LIBCM_PLUS        (1 << 2)
#define LIBCM_SPACE       (1 << 3)
#define LIBCM_LEFT        (1 << 4)
#define LIBCM_SPECIAL     (1 << 5)
#define LIBCM_LARGE       (1 << 6)

#define LIBCM_ISDIGIT(c)  ((unsigned)((c) - '0') < 10)

int libcm_vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
int libcm_snprintf(char *buf, size_t size, const char *fmt, ...);
int libcm_printf(const char *fmt, ...);
int libcm_vprintf(const char *fmt, va_list args);

void libcm_putchar(char c);
void libcm_puts(const char *str);

size_t libcm_strlen(const char *s);
char *libcm_strcpy(char *dest, const char *src);
char *libcm_strcat(char *dest, const char *src);
char *libcm_strdup(const char *s);
char *libcm_strncpy(char *dest, const char *src, size_t n);
int libcm_strcmp(const char *s1, const char *s2);
int libcm_strncmp(const char *s1, const char *s2, size_t n);
int libcm_memcmp(const void *s1, const void *s2, size_t n);
void *libcm_memcpy(void *dest, const void *src, size_t n);
void *libcm_memset(void *s, int c, size_t n);
void *libcm_malloc(size_t size);
void libcm_free(void *ptr);
void *libcm_calloc(size_t nmemb, size_t size);
void *libcm_realloc(void *ptr, size_t size);
unsigned long libcm_strtoul(const char *nptr, char **endptr, int base);

__attribute__((weak)) int libcm_console_output(const char *str, int len);

#endif
