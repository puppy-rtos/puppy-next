#include <puppy_core.h>
#include "puppy_log.h"
#include "pup_sem.h"
#include "kheap.h"

typedef struct {
    sem_t   sem;
    char    name[PUP_SEM_NAME_MAX];
    int     ref_count;
    int     valid;
} pup_named_sem_t;

static pup_named_sem_t *_sem_pool = PUP_NULL;

static int _sem_pool_init(void)
{
    pup_named_sem_t *mem;
    pup_base_t key;
    int i;

    if (_sem_pool != PUP_NULL) {
        return 0;
    }

    mem = (pup_named_sem_t *)kheap_malloc(sizeof(pup_named_sem_t) * PUP_SEM_MAX);
    if (mem == PUP_NULL) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_IPC, "pup_sem: alloc pool failed");
        return -1;
    }

    key = arch_irq_lock();
    if (_sem_pool == PUP_NULL) {
        _sem_pool = mem;
        for (i = 0; i < PUP_SEM_MAX; i++) {
            _sem_pool[i].name[0] = '\0';
            _sem_pool[i].ref_count = 0;
            _sem_pool[i].valid = 0;
        }
        mem = PUP_NULL;
        PUP_LOG_INFO(PUP_LOG_MODULE_IPC, "pup_sem: pool allocated max=%d", PUP_SEM_MAX);
    }
    arch_irq_unlock(key);

    if (mem != PUP_NULL) {
        kheap_free(mem);
    }

    return 0;
}

static int _name_eq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (*a == '\0' && *b == '\0');
}

static void _name_copy(char *dst, const char *src)
{
    int i = 0;
    while (i < PUP_SEM_NAME_MAX - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int pup_sem_open(const char *name, int flags, unsigned int value)
{
    pup_base_t key;
    int i, free_slot = -1;

    if (_sem_pool_init() != 0) {
        return -1;
    }

    if (!name || name[0] == '\0') {
        return -1;
    }

    key = arch_irq_lock();

    for (i = 0; i < PUP_SEM_MAX; i++) {
        if (_sem_pool[i].valid) {
            if (_name_eq(_sem_pool[i].name, name)) {
                if ((flags & PUP_SEM_CREATE) && (flags & PUP_SEM_EXCL)) {
                    arch_irq_unlock(key);
                    PUP_LOG_ERROR(PUP_LOG_MODULE_IPC, "pup_sem_open: '%s' exists (EXCL)", name);
                    return -1;
                }
                _sem_pool[i].ref_count++;
                arch_irq_unlock(key);
                PUP_LOG_DEBUG(PUP_LOG_MODULE_IPC, "pup_sem_open: '%s' attach ref=%d", name, _sem_pool[i].ref_count);
                return i;
            }
        } else if (free_slot < 0) {
            free_slot = i;
        }
    }

    if (!(flags & PUP_SEM_CREATE)) {
        arch_irq_unlock(key);
        PUP_LOG_ERROR(PUP_LOG_MODULE_IPC, "pup_sem_open: '%s' not found", name);
        return -1;
    }

    if (free_slot < 0) {
        arch_irq_unlock(key);
        PUP_LOG_ERROR(PUP_LOG_MODULE_IPC, "pup_sem_open: pool full");
        return -1;
    }

    _name_copy(_sem_pool[free_slot].name, name);
    _sem_pool[free_slot].valid     = 1;
    _sem_pool[free_slot].ref_count = 1;
    sem_init(&_sem_pool[free_slot].sem, 1, value);

    arch_irq_unlock(key);
    PUP_LOG_INFO(PUP_LOG_MODULE_IPC, "pup_sem_open: '%s' created slot=%d val=%u", name, free_slot, value);
    return free_slot;
}

int pup_sem_close(int handle)
{
    pup_base_t key;

    if (_sem_pool == PUP_NULL) {
        return -1;
    }

    if (handle < 0 || handle >= PUP_SEM_MAX) {
        return -1;
    }

    key = arch_irq_lock();

    if (!_sem_pool[handle].valid) {
        arch_irq_unlock(key);
        return -1;
    }

    _sem_pool[handle].ref_count--;
    if (_sem_pool[handle].ref_count <= 0) {
        sem_destroy(&_sem_pool[handle].sem);
        _sem_pool[handle].valid     = 0;
        _sem_pool[handle].name[0]   = '\0';
        _sem_pool[handle].ref_count = 0;
        PUP_LOG_INFO(PUP_LOG_MODULE_IPC, "pup_sem_close: slot %d destroyed", handle);
    } else {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_IPC, "pup_sem_close: slot %d ref=%d", handle, _sem_pool[handle].ref_count);
    }

    arch_irq_unlock(key);
    return 0;
}

int pup_sem_wait(int handle)
{
    if (_sem_pool == PUP_NULL) {
        return -1;
    }

    if (handle < 0 || handle >= PUP_SEM_MAX || !_sem_pool[handle].valid) {
        return -1;
    }
    PUP_LOG_DEBUG(PUP_LOG_MODULE_IPC, "pup_sem_wait: slot %d", handle);
    return sem_wait(&_sem_pool[handle].sem);
}

int pup_sem_trywait(int handle)
{
    if (_sem_pool == PUP_NULL) {
        return -1;
    }

    if (handle < 0 || handle >= PUP_SEM_MAX || !_sem_pool[handle].valid) {
        return -1;
    }
    return sem_trywait(&_sem_pool[handle].sem);
}

int pup_sem_post(int handle)
{
    if (_sem_pool == PUP_NULL) {
        return -1;
    }

    if (handle < 0 || handle >= PUP_SEM_MAX || !_sem_pool[handle].valid) {
        return -1;
    }
    PUP_LOG_DEBUG(PUP_LOG_MODULE_IPC, "pup_sem_post: slot %d", handle);
    return sem_post(&_sem_pool[handle].sem);
}
