#include "libcm.h"
#include "kheap.h"
#include <stdint.h>

static char _printk_buf[LIBCM_PRINTK_BUF_SIZE];

__attribute__((weak)) int libcm_console_output(const char *str, int len)
{
    return 0;
}

void libcm_putchar(char c)
{
    libcm_console_output(&c, 1);
}

void libcm_puts(const char *str)
{
    int len = libcm_strlen(str);
    libcm_console_output(str, len);
}

size_t libcm_strlen(const char *s)
{
    const char *sc;
    for (sc = s; *sc != '\0'; ++sc)
        ;
    return sc - s;
}

char *libcm_strcpy(char *dest, const char *src)
{
    char *tmp = dest;
    while ((*dest++ = *src++) != '\0')
        ;
    return tmp;
}

char *libcm_strcat(char *dest, const char *src)
{
    char *tmp = dest;
    while (*dest)
        dest++;
    while ((*dest++ = *src++) != '\0')
        ;
    return tmp;
}

int libcm_strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int libcm_strncmp(const char *s1, const char *s2, size_t n)
{
    if (n == 0) {
        return 0;
    }
    
    while (n-- > 0 && *s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    
    if (n == (size_t)-1) {
        return 0;
    }
    
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int libcm_memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2)
            return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

void *libcm_memcpy(void *dest, const void *src, size_t n)
{
    char *tmp = dest;
    const char *s = src;
    while (n--)
        *tmp++ = *s++;
    return dest;
}

void *libcm_memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

static inline int divide(long *n, int base)
{
    int res;

    res = (int)(((unsigned long)*n) % base);
    *n = (long)(((unsigned long)*n) / base);

    return res;
}

static inline int skip_atoi(const char **s)
{
    int i = 0;
    while (LIBCM_ISDIGIT(**s))
        i = i * 10 + *((*s)++) - '0';

    return i;
}

static char *print_number(char *buf,
                          char *end,
                          long  num,
                          int   base,
                          int   s,
                          int   precision,
                          int   type)
{
    char c, sign;
    char tmp[32];
    int precision_bak = precision;
    const char *digits;
    static const char small_digits[] = "0123456789abcdef";
    static const char large_digits[] = "0123456789ABCDEF";
    int i, size;

    size = s;

    digits = (type & LIBCM_LARGE) ? large_digits : small_digits;
    if (type & LIBCM_LEFT)
        type &= ~LIBCM_ZEROPAD;

    c = (type & LIBCM_ZEROPAD) ? '0' : ' ';

    sign = 0;
    if (type & LIBCM_SIGN)
    {
        if (num < 0)
        {
            sign = '-';
            num = -num;
        }
        else if (type & LIBCM_PLUS)
            sign = '+';
        else if (type & LIBCM_SPACE)
            sign = ' ';
    }

    i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else
    {
        while (num != 0)
            tmp[i++] = digits[divide(&num, base)];
    }

    if (i > precision)
        precision = i;
    size -= precision;

    if (!(type & (LIBCM_ZEROPAD | LIBCM_LEFT)))
    {
        if ((sign) && (size > 0))
            size--;

        while (size-- > 0)
        {
            if (buf < end)
                *buf = ' ';
            ++ buf;
        }
    }

    if (sign)
    {
        if (buf < end)
        {
            *buf = sign;
        }
        -- size;
        ++ buf;
    }

    if (!(type & LIBCM_LEFT))
    {
        while (size-- > 0)
        {
            if (buf < end)
                *buf = c;
            ++ buf;
        }
    }

    while (i < precision--)
    {
        if (buf < end)
            *buf = '0';
        ++ buf;
    }

    while (i-- > 0 && (precision_bak != 0))
    {
        if (buf < end)
            *buf = tmp[i];
        ++ buf;
    }

    while (size-- > 0)
    {
        if (buf < end)
            *buf = ' ';
        ++ buf;
    }

    return buf;
}

int libcm_vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    uint32_t num;
    int i, len;
    char *str, *end, c;
    const char *s;

    uint8_t base;
    uint8_t flags;
    uint8_t qualifier;
    int32_t field_width;
    int precision;

    str = buf;
    end = buf + size;

    if (end < buf)
    {
        end  = ((char *) - 1);
        size = end - buf;
    }

    for (; *fmt ; ++fmt)
    {
        if (*fmt != '%')
        {
            if (str < end)
                *str = *fmt;
            ++ str;
            continue;
        }

        flags = 0;

        while (1)
        {
            ++ fmt;
            if (*fmt == '-') flags |= LIBCM_LEFT;
            else if (*fmt == '+') flags |= LIBCM_PLUS;
            else if (*fmt == ' ') flags |= LIBCM_SPACE;
            else if (*fmt == '#') flags |= LIBCM_SPECIAL;
            else if (*fmt == '0') flags |= LIBCM_ZEROPAD;
            else break;
        }

        field_width = -1;
        if (LIBCM_ISDIGIT(*fmt)) field_width = skip_atoi(&fmt);
        else if (*fmt == '*')
        {
            ++ fmt;
            field_width = va_arg(args, int);
            if (field_width < 0)
            {
                field_width = -field_width;
                flags |= LIBCM_LEFT;
            }
        }

        precision = -1;
        if (*fmt == '.')
        {
            ++ fmt;
            if (LIBCM_ISDIGIT(*fmt)) precision = skip_atoi(&fmt);
            else if (*fmt == '*')
            {
                ++ fmt;
                precision = va_arg(args, int);
            }
            if (precision < 0) precision = 0;
        }

        qualifier = 0;
        if (*fmt == 'h' || *fmt == 'l')
        {
            qualifier = *fmt;
            ++ fmt;
        }

        base = 10;

        switch (*fmt)
        {
        case 'c':
            if (!(flags & LIBCM_LEFT))
            {
                while (--field_width > 0)
                {
                    if (str < end) *str = ' ';
                    ++ str;
                }
            }

            c = (uint8_t)va_arg(args, int);
            if (str < end) *str = c;
            ++ str;

            while (--field_width > 0)
            {
                if (str < end) *str = ' ';
                ++ str;
            }
            continue;

        case 's':
            s = va_arg(args, char *);
            if (!s) s = "(NULL)";

            for (len = 0; (len != field_width) && (s[len] != '\0'); len++);
            if (precision > 0 && len > precision) len = precision;

            if (!(flags & LIBCM_LEFT))
            {
                while (len < field_width--)
                {
                    if (str < end) *str = ' ';
                    ++ str;
                }
            }

            for (i = 0; i < len; ++i)
            {
                if (str < end) *str = *s;
                ++ str;
                ++ s;
            }

            while (len < field_width--)
            {
                if (str < end) *str = ' ';
                ++ str;
            }
            continue;

        case 'p':
            if (field_width == -1)
            {
                field_width = sizeof(void *) << 1;
                flags |= LIBCM_ZEROPAD;
            }
            str = print_number(str, end,
                               (long)va_arg(args, void *),
                               16, field_width, precision, flags);
            continue;

        case '%':
            if (str < end) *str = '%';
            ++ str;
            continue;

        case 'b':
            base = 2;
            break;
        case 'o':
            base = 8;
            break;

        case 'X':
            flags |= LIBCM_LARGE;
        case 'x':
            base = 16;
            break;

        case 'd':
        case 'i':
            flags |= LIBCM_SIGN;
        case 'u':
            break;

        default:
            if (str < end) *str = '%';
            ++ str;

            if (*fmt)
            {
                if (str < end) *str = *fmt;
                ++ str;
            }
            else
            {
                -- fmt;
            }
            continue;
        }

        if (qualifier == 'l')
        {
            num = va_arg(args, uint32_t);
            if (flags & LIBCM_SIGN) num = (int32_t)num;
        }
        else if (qualifier == 'h')
        {
            num = (uint16_t)va_arg(args, int);
            if (flags & LIBCM_SIGN) num = (int16_t)num;
        }
        else
        {
            num = va_arg(args, uint32_t);
            if (flags & LIBCM_SIGN) num = (int32_t)num;
        }
        str = print_number(str, end, num, base, field_width, precision, flags);
    }

    if (size > 0)
    {
        if (str < end) *str = '\0';
        else
        {
            end[-1] = '\0';
        }
    }

    return str - buf;
}

int libcm_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = libcm_vsnprintf(buf, size, fmt, args);
    va_end(args);

    return n;
}

int libcm_vprintf(const char *fmt, va_list args)
{
    int n;

    n = libcm_vsnprintf(_printk_buf, LIBCM_PRINTK_BUF_SIZE, fmt, args);

    libcm_console_output(_printk_buf, n);

    return n;
}

int libcm_printf(const char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = libcm_vprintf(fmt, args);
    va_end(args);

    return n;
}

unsigned long libcm_strtoul(const char *nptr, char **endptr, int base)
{
    const char *s = nptr;
    unsigned long acc;
    int c;
    unsigned long cutoff;
    int neg = 0, any, cutlim;

    do {
        c = *s++;
    } while (c == ' ' || c == '\t');

    if (c == '-') {
        neg = 1;
        c = *s++;
    } else if (c == '+') {
        c = *s++;
    }

    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1];
        s += 2;
        base = 16;
    }

    if (base == 0) {
        base = c == '0' ? 8 : 10;
    }

    cutoff = (unsigned long)0xFFFFFFFFUL / (unsigned long)base;
    cutlim = (unsigned long)0xFFFFFFFFUL % (unsigned long)base;

    for (acc = 0, any = 0;; c = *s++) {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'a' && c <= 'z') {
            c -= 'a' - 10;
        } else if (c >= 'A' && c <= 'Z') {
            c -= 'A' - 10;
        } else {
            break;
        }

        if (c >= base) {
            break;
        }

        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }

    if (any < 0) {
        acc = 0xFFFFFFFFUL;
    } else if (neg) {
        acc = -acc;
    }

    if (endptr != 0) {
        *endptr = (char *)(any ? s - 1 : nptr);
    }

    return acc;
}

void *libcm_malloc(size_t size)
{
    return kheap_malloc(size);
}

void libcm_free(void *ptr)
{
    kheap_free(ptr);
}

void *libcm_calloc(size_t nmemb, size_t size)
{
    return kheap_calloc(nmemb, size);
}

void *libcm_realloc(void *ptr, size_t size)
{
    return kheap_realloc(ptr, size);
}

char *libcm_strdup(const char *s)
{
    if (s == NULL) {
        return NULL;
    }
    
    size_t len = libcm_strlen(s) + 1;
    char *new_str = (char *)libcm_malloc(len);
    
    if (new_str) {
        libcm_memcpy(new_str, s, len);
    }
    
    return new_str;
}

char *libcm_strncpy(char *dest, const char *src, size_t n)
{
    char *tmp = dest;
    
    while (n > 0 && *src != '\0') {
        *tmp++ = *src++;
        n--;
    }
    
    while (n > 0) {
        *tmp++ = '\0';
        n--;
    }
    
    return dest;
}
