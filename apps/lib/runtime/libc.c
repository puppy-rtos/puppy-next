#include "libc.h"
#include "syscall.h"
#include <stdint.h>

#define USER_LIBC_PRINTK_BUF_SIZE 256

static char _printk_buf[USER_LIBC_PRINTK_BUF_SIZE];

int pthread_attr_init(pthread_attr_t* attr) {
    if (!attr) return -1;
    attr->name = "pthread";
    attr->stackaddr = 0;
    attr->stacksize = 1024;
    attr->priority = 12;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t* attr) {
    return 0;
}

int pthread_attr_setstackaddr(pthread_attr_t* attr, void* stackaddr) {
    if (!attr) return -1;
    attr->stackaddr = stackaddr;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize) {
    if (!attr) return -1;
    attr->stacksize = stacksize;
    return 0;
}

int pthread_attr_setname(pthread_attr_t* attr, const char* name) {
    if (!attr) return -1;
    attr->name = name;
    return 0;
}

int pthread_attr_setpriority(pthread_attr_t* attr, int priority) {
    if (!attr) return -1;
    attr->priority = priority;
    return 0;
}

int pthread_create(pthread_t* thread, pthread_attr_t* attr, 
                   void* (*start_routine)(void*), void* arg) {
    pup_user_thread_create_t params;

    if (!thread || !attr || !start_routine) return -1;
    if (!attr->stackaddr) return -1;

    params.arg = arg;
    params.stackaddr = attr->stackaddr;
    params.stacksize = attr->stacksize;
    params.priority = attr->priority;
    params.name = attr->name;

    int tid = thread_create(start_routine, &params);
    if (tid < 0) return -1;
    
    *thread = (pthread_t)(intptr_t)tid;
    return 0;
}

void pthread_exit(void* retval) {
    exit((int)(intptr_t)retval);
}

pthread_t pthread_self(void) {
    return (pthread_t)0;
}

int pthread_join(pthread_t thread, void** retval) {
    int tid = (int)(intptr_t)thread;
    return thread_join(tid);
}

int pthread_delay(int ticks) {
    return delay(ticks);
}

pup_tick_t pup_tick_get(void) {
    return (pup_tick_t)tick_get();
}

int sched_yield(void) {
    sys_yield();
    return 0;
}

static inline int divide(long* n, int base) {
    int res;
    res = (int)(((unsigned long)*n) % base);
    *n = (long)(((unsigned long)*n) / base);
    return res;
}

static inline int skip_atoi(const char** s) {
    int i = 0;
    while (((unsigned)((**s) - '0') < 10))
        i = i * 10 + *((*s)++) - '0';
    return i;
}

#define ZEROPAD   (1 << 0)
#define SIGN      (1 << 1)
#define PLUS      (1 << 2)
#define SPACE     (1 << 3)
#define LEFT      (1 << 4)
#define SPECIAL   (1 << 5)
#define LARGE     (1 << 6)

#define ISDIGIT(c) ((unsigned)((c) - '0') < 10)

static char* print_number(char* buf, char* end, long num, int base, 
                          int s, int precision, int type) {
    char c, sign;
    char tmp[32];
    int precision_bak = precision;
    const char* digits;
    static const char small_digits[] = "0123456789abcdef";
    static const char large_digits[] = "0123456789ABCDEF";
    int i, size;

    size = s;
    digits = (type & LARGE) ? large_digits : small_digits;
    if (type & LEFT) type &= ~ZEROPAD;
    
    c = (type & ZEROPAD) ? '0' : ' ';
    sign = 0;
    
    if (type & SIGN) {
        if (num < 0) {
            sign = '-';
            num = -num;
        } else if (type & PLUS) {
            sign = '+';
        } else if (type & SPACE) {
            sign = ' ';
        }
    }

    i = 0;
    if (num == 0) {
        tmp[i++] = '0';
    } else {
        while (num != 0)
            tmp[i++] = digits[divide(&num, base)];
    }
    
    if (i > precision) precision = i;
    size -= precision;

    if (!(type & (ZEROPAD | LEFT))) {
        if ((sign) && (size > 0)) size--;
        while (size-- > 0) {
            if (buf < end) *buf = ' ';
            ++buf;
        }
    }

    if (sign) {
        if (buf < end) *buf = sign;
        --size;
        ++buf;
    }

    if (!(type & LEFT)) {
        while (size-- > 0) {
            if (buf < end) *buf = c;
            ++buf;
        }
    }

    while (i < precision--) {
        if (buf < end) *buf = '0';
        ++buf;
    }

    while (i-- > 0 && (precision_bak != 0)) {
        if (buf < end) *buf = tmp[i];
        ++buf;
    }

    while (size-- > 0) {
        if (buf < end) *buf = ' ';
        ++buf;
    }

    return buf;
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args) {
    uint32_t num;
    int i, len;
    char *str, *end, c;
    const char* s;
    uint8_t base;
    uint8_t flags;
    uint8_t qualifier;
    int32_t field_width;
    int precision;

    str = buf;
    end = buf + size;

    if (end < buf) {
        end = ((char*)-1);
        size = end - buf;
    }

    for (; *fmt; ++fmt) {
        if (*fmt != '%') {
            if (str < end) *str = *fmt;
            ++str;
            continue;
        }

        flags = 0;
        while (1) {
            ++fmt;
            if (*fmt == '-') flags |= LEFT;
            else if (*fmt == '+') flags |= PLUS;
            else if (*fmt == ' ') flags |= SPACE;
            else if (*fmt == '#') flags |= SPECIAL;
            else if (*fmt == '0') flags |= ZEROPAD;
            else break;
        }

        field_width = -1;
        if (ISDIGIT(*fmt)) {
            field_width = skip_atoi(&fmt);
        } else if (*fmt == '*') {
            ++fmt;
            field_width = va_arg(args, int);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        precision = -1;
        if (*fmt == '.') {
            ++fmt;
            if (ISDIGIT(*fmt)) {
                precision = skip_atoi(&fmt);
            } else if (*fmt == '*') {
                ++fmt;
                precision = va_arg(args, int);
            }
            if (precision < 0) precision = 0;
        }

        qualifier = 0;
        if (*fmt == 'h' || *fmt == 'l') {
            qualifier = *fmt;
            ++fmt;
        }

        base = 10;

        switch (*fmt) {
        case 'c':
            if (!(flags & LEFT)) {
                while (--field_width > 0) {
                    if (str < end) *str = ' ';
                    ++str;
                }
            }
            c = (uint8_t)va_arg(args, int);
            if (str < end) *str = c;
            ++str;
            while (--field_width > 0) {
                if (str < end) *str = ' ';
                ++str;
            }
            continue;

        case 's':
            s = va_arg(args, char*);
            if (!s) s = "(NULL)";
            for (len = 0; (len != field_width) && (s[len] != '\0'); len++);
            if (precision > 0 && len > precision) len = precision;
            
            if (!(flags & LEFT)) {
                while (len < field_width--) {
                    if (str < end) *str = ' ';
                    ++str;
                }
            }
            for (i = 0; i < len; ++i) {
                if (str < end) *str = *s;
                ++str;
                ++s;
            }
            while (len < field_width--) {
                if (str < end) *str = ' ';
                ++str;
            }
            continue;

        case 'p':
            if (field_width == -1) {
                field_width = sizeof(void*) << 1;
                flags |= ZEROPAD;
            }
            str = print_number(str, end, (long)va_arg(args, void*),
                             16, field_width, precision, flags);
            continue;

        case '%':
            if (str < end) *str = '%';
            ++str;
            continue;

        case 'b':
            base = 2;
            break;
        case 'o':
            base = 8;
            break;
        case 'X':
            flags |= LARGE;
        case 'x':
            base = 16;
            break;
        case 'd':
        case 'i':
            flags |= SIGN;
        case 'u':
            break;

        default:
            if (str < end) *str = '%';
            ++str;
            if (*fmt) {
                if (str < end) *str = *fmt;
                ++str;
            } else {
                --fmt;
            }
            continue;
        }

        if (qualifier == 'l') {
            num = va_arg(args, uint32_t);
            if (flags & SIGN) num = (int32_t)num;
        } else if (qualifier == 'h') {
            num = (uint16_t)va_arg(args, int);
            if (flags & SIGN) num = (int16_t)num;
        } else {
            num = va_arg(args, uint32_t);
            if (flags & SIGN) num = (int32_t)num;
        }
        str = print_number(str, end, num, base, field_width, precision, flags);
    }

    if (size > 0) {
        if (str < end) *str = '\0';
        else end[-1] = '\0';
    }

    return str - buf;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list args;
    int n;
    va_start(args, fmt);
    n = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return n;
}

int vprintf(const char* fmt, va_list args) {
    int n = vsnprintf(_printk_buf, USER_LIBC_PRINTK_BUF_SIZE, fmt, args);
    write(STDOUT_FILENO, _printk_buf, n);
    return n;
}

int printf(const char* fmt, ...) {
    va_list args;
    int n;
    va_start(args, fmt);
    n = vprintf(fmt, args);
    va_end(args);
    return n;
}

size_t strlen(const char* s) {
    const char* sc;
    for (sc = s; *sc != '\0'; ++sc)
        ;
    return sc - s;
}

char* strcpy(char* dest, const char* src) {
    char* tmp = dest;
    while ((*dest++ = *src++) != '\0')
        ;
    return tmp;
}

char* strcat(char* dest, const char* src) {
    char* tmp = dest;
    while (*dest)
        dest++;
    while ((*dest++ = *src++) != '\0')
        ;
    return tmp;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    char* tmp = dest;
    const char* s = src;
    while (n--)
        *tmp++ = *s++;
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2)
            return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}
