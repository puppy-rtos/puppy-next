/*
 * Copyright (c) 2022-2023, The Puppy RTOS Authors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <puppy_core.h>
#include <string.h>
#include "puppy_log.h"
#include "kheap.h"

#ifdef PUP_DEBUG
#define PUP_SCHED_GUARD_ENABLED 1
#else
#define PUP_SCHED_GUARD_ENABLED 0
#endif

 /**
  * @brief static structure declaration
  */

static pup_list_t _g_pobj_list   = { &_g_pobj_list,   &_g_pobj_list   };
static pup_list_t _g_zombie_list = { &_g_zombie_list, &_g_zombie_list };

/**
 * @brief tick system for time slice scheduling
 */
static pup_tick_t _g_tick = 0;
static pup_list_t _g_timeout_list = { &_g_timeout_list, &_g_timeout_list };
static volatile pup_uint8_t _g_sched_runtime_ready = 0;

pup_tick_t pup_tick_get(void)
{
    return _g_tick;
}

const pthread_attr_t pthread_default_attr = {
    "pthread",
    PUP_SCHED_PRIO_DEFAULT,
    PTHREAD_CREATE_JOINABLE, /* detach state */
    0,
    PUP_CONFIG_PTHREAD_STACK_DEFAULT,
};

struct _pthread_obj {
    pthread_attr_t attr;

    pup_uint8_t state;
    pup_uint8_t prio;
    pup_node_t tnode;
    pup_node_t link_node;

    void* entry;
    void* param;
    void* stackaddr;      /* Address of memory to be used as stack */
    pup_size_t stacksize; /* Size of the stack allocated for the pthread */
    int errno;

    void* cleanup;
#if PUP_CPU_NR > 1
    pup_uint8_t bindcpu;
    pup_uint8_t oncpu;
#endif
    /* exit value buf */
    void** exit_value;
    struct _pthread_obj* join_thread;

    /** timeout for delay */
    pup_tick_t timeout_tick;

    /** arch-specifics */
    void* arch_data;
};
#define CPU_NA ((pup_uint8_t) - 1)

struct pup_cpu {
    pup_list_t ready_queue;
    pup_list_t dead_queue;
    struct _pthread_obj* curr_thread;
    struct _pthread_obj* next_thread;
    pup_ubase_t sched_lock;
};

/**
 * @brief static function declaration
 */
static int pup_sched_ready_remove(struct _pthread_obj* thread);
static int pup_sched_ready_insert(struct _pthread_obj* thread);

/**
 * pup_container_of - return the start address of struct type, while ptr is the
 * member of struct type.
 */
#define pup_container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

 /**
  * @brief get the struct for this entry
  * @param node the entry point
  * @param type the type of structure
  * @param member the name of list in structure
  */
#define pup_list_entry(node, type, member) \
    pup_container_of(node, type, member)

  /**
   * @brief Provide the primitive to iterate on a list
   * Note: the loop is unsafe and thus node should not be removed
   * @param list A pointer on a pup_list_t to iterate on.
   * @param node A pup_node_t pointer to peek each node of the list
   */
#define pup_list_for_each_node(list, node) \
    for (node = (list)->next; node != (list); node = node->next)

   /**
    * @brief Provide the primitive to safely iterate on a list
    * Note: node can be removed, it will not break the loop.
    * @param list A pointer on a pup_list_t to iterate on.
    * @param node A pup_node_t pointer to peek each node of the list
    * @param node_s A pup_node_t pointer for the loop to run safely
    */
#define pup_list_for_each_node_safe(list, node, node_s)            \
    for (node = (list)->next, node_s = node->next; node != (list); \
         node = node_s, node_s = node->next)

#define PUP_LIST_STATIC_INIT(list_ptr) \
    {                                  \
        {(list_ptr)}, { (list_ptr) }   \
    }

static inline void pup_list_init(pup_list_t* list) {
    list->next = list->prev = list;
}

static inline pup_uint8_t pup_list_is_empty(pup_list_t* list) {
    return list->next == list;
}

static inline pup_uint8_t pup_node_is_linked(pup_node_t* node) {
    return node->next != PUP_NULL;
}

static inline pup_uint8_t pup_list_is_sane(pup_list_t* list) {
    if (!list || !list->next || !list->prev) {
        return 0;
    }
    if (list->next->prev != list || list->prev->next != list) {
        return 0;
    }
    return 1;
}

/**
 * @brief add node to tail of list
 *
 * This and other pup_list_*() functions are not thread safe.
 *
 * @param list the doubly-linked list to operate on
 * @param node the element to append
 */

static inline void pup_list_append(pup_list_t* list, pup_node_t* node) {
    pup_node_t* const tail = list->prev;

    node->next = list;
    node->prev = tail;

    tail->next = node;
    list->prev = node;
}

/**
 * @brief add node to head of list
 *
 * This and other pup_list_*() functions are not thread safe.
 *
 * @param list the doubly-linked list to operate on
 * @param node the element to append
 */

static inline void pup_list_prepend(pup_list_t* list, pup_node_t* node) {
    pup_node_t* const head = list->next;

    node->next = head;
    node->prev = list;

    head->prev = node;
    list->next = node;
}

/**
 * @brief Insert a node into a list
 *
 * Insert a node before a specified node in a dlist.
 *
 * @param successor the position before which "node" will be inserted
 * @param node the element to insert
 */
static inline void pup_list_insert(pup_node_t* successor, pup_node_t* node) {
    pup_node_t* const prev = successor->prev;

    node->prev = prev;
    node->next = successor;
    prev->next = node;
    successor->prev = node;
}

/**
 * @brief remove node from list.
 * @param node the node to remove from the list.
 */
static inline void pup_list_remove(pup_node_t* node) {
    node->next->prev = node->prev;
    node->prev->next = node->next;

    node->prev = node->next = PUP_NULL;
}

/**
 * @addtogroup cpu
 * @{
 */

#ifndef PUP_GET_CPU_ID
#define PUP_GET_CPU_ID() 0
#endif

static struct pup_cpu _g_cpu[PUP_CPU_NR];

pup_weak void arch_sched_init(void) {
}

struct pup_cpu* pup_cpu_self(void) {
    return &_g_cpu[PUP_GET_CPU_ID()];
}

struct pup_cpu* pup_cpu_index(pup_uint8_t cpuid) {
    if (cpuid >= PUP_CPU_NR)
        return PUP_NULL;
    return &_g_cpu[cpuid];
}
arch_spinlock_t _g_cpu_lock;
// idle thread stack for each cpu
static pup_uint8_t _g_idle_thread_stack[PUP_CPU_NR][PUP_IDLE_THREAD_STACK_SIZE] __attribute__((aligned(PUP_ALIGN_SIZE)));
static void* _pup_idle_thread(void* param) {
    while (1) {
        __asm("wfi");
    }
    return PUP_NULL;
}

void pup_cpu_init(void) {
    arch_spin_lock_init(&_g_cpu_lock);
    arch_sched_init();
    for (int i = 0; i < PUP_CPU_NR; i++) {
        pup_list_init(&_g_cpu[i].ready_queue);
        pup_list_init(&_g_cpu[i].dead_queue);
        _g_cpu[i].sched_lock = 1;

        // create idle thread
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setpriority(&attr, PUP_THREAD_PRIO_MAX);
        pthread_attr_setstacksize(&attr, PUP_IDLE_THREAD_STACK_SIZE);
        pthread_attr_setstackaddr(&attr, _g_idle_thread_stack[i]);
        pthread_attr_setname(&attr, "idle");
#if PUP_CPU_NR > 1
        pthread_attr_setcpu(&attr, i);
#endif
        pthread_create(PUP_NULL, &attr, _pup_idle_thread, PUP_NULL);
    }
}

/**@}*/

static int _g_errno;

int pup_get_errno(void) {
    struct _pthread_obj* _thread = pthread_self();
    if (_thread)
        return _thread->errno;
    else
        return _g_errno;
}

/**
 * @addtogroup sched
 * @{
 */

struct _pthread_obj* pup_sched_ready_highest(void);

static int pup_thread_ptr_valid(struct _pthread_obj* thread) {
    unsigned int addr = (unsigned int)thread;
    if (!thread) {
        return 0;
    }
    if (addr < PUP_RAM_ADDRESS_START || addr >= PUP_RAM_ADDRESS_END) {
        return 0;
    }
    return 1;
}

void pup_sched_swap_done_cb(void) {
    struct pup_cpu* cpu = pup_cpu_self();

#if PUP_SCHED_GUARD_ENABLED
    if (!cpu) {
        return;
    }

    if (!pup_thread_ptr_valid(cpu->next_thread)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SCHED, "swap done with invalid next_thread: 0x%08x", (unsigned int)cpu->next_thread);
        cpu->next_thread = PUP_NULL;
        return;
    }
#endif

    cpu->next_thread->state = PUP_THREAD_STATE_RUN;
    cpu->curr_thread = cpu->next_thread;
    cpu->next_thread = PUP_NULL;
}

int pup_sched(void) {
    int ret = 0;
    struct _pthread_obj* _h_thread;
    pup_base_t key = arch_irq_lock();
    struct pup_cpu* _cpu = pup_cpu_self();

    if (_cpu->sched_lock == 0) {
        if (!_cpu->next_thread) {
            /* get prio higest thread */
            _h_thread = pup_sched_ready_highest();
            if (!_h_thread) {
                goto _exit;
            }
            if (_cpu->curr_thread && _cpu->curr_thread->state == PUP_THREAD_STATE_RUN && _h_thread->prio >= _cpu->curr_thread->prio) {
                goto _exit;
            }
            _cpu->next_thread = _h_thread;
            pup_sched_ready_remove(_h_thread);
            if (_cpu->curr_thread && _cpu->curr_thread->state == PUP_THREAD_STATE_RUN) {
                pup_sched_ready_insert(_cpu->curr_thread);
            }
        }
    #if PUP_SCHED_GUARD_ENABLED
        if (!pup_thread_ptr_valid(_cpu->next_thread)) {
            PUP_LOG_ERROR(PUP_LOG_MODULE_SCHED, "sched got invalid next_thread: 0x%08x", (unsigned int)_cpu->next_thread);
            _cpu->next_thread = PUP_NULL;
            goto _exit;
        }
    #endif

        if (_cpu->curr_thread) {
            arch_swap(_cpu->curr_thread->arch_data, _cpu->next_thread->arch_data);
        } else {
            arch_swap(PUP_NULL, _cpu->next_thread->arch_data);
        }
    }
_exit:
    arch_irq_unlock(key);
    return ret;
}

void pup_sched_lock(void) {
    pup_base_t key = arch_irq_lock();
    pup_cpu_self()->sched_lock += 1;
    arch_irq_unlock(key);
}
void pup_sched_unlock(void) {
    pup_base_t key = arch_irq_lock();
    pup_cpu_self()->sched_lock -= 1;
    arch_irq_unlock(key);
    if (!pup_cpu_self()->sched_lock) {
        pup_sched();
    }
}

int sched_getcpu(void) {
    return PUP_GET_CPU_ID();
}

static int pup_sched_ready_insert(struct _pthread_obj* thread) {
    static pup_uint8_t cpuid_last = 0;
    struct _pthread_obj* old_thread = PUP_NULL, * temp_thread = PUP_NULL, * _thread = thread;
    pup_base_t key = arch_irq_lock();
    arch_spin_lock(&_g_cpu_lock);

    pup_node_t* node;
    pup_list_t* _ready_queue;
    PUP_ASSERT(_thread != PUP_NULL);
    PUP_ASSERT(_thread->state != PUP_THREAD_STATE_READY);

#if PUP_CPU_NR > 1
    pup_uint8_t need_send = CPU_NA;
    if (_thread->oncpu != CPU_NA) {
        cpuid_last = _thread->oncpu;
    } else if (_thread->bindcpu != CPU_NA) {
        cpuid_last = _thread->bindcpu;
    }
#endif
    _ready_queue = &pup_cpu_index(cpuid_last)->ready_queue;

    if (PUP_SCHED_GUARD_ENABLED && !pup_list_is_sane(_ready_queue)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SCHED, "ready_queue corrupted before insert, reinit");
        pup_list_init(_ready_queue);
    }

    pup_list_for_each_node(_ready_queue, node) {
        temp_thread = pup_list_entry(node, struct _pthread_obj, tnode);
        if (temp_thread->prio > _thread->prio) {
            /* find out insert node */
            old_thread = temp_thread;
            break;
        }
    }

    _thread->state = PUP_THREAD_STATE_READY;
    if (old_thread) {
        pup_list_insert(&old_thread->tnode, &_thread->tnode);
    } else {
        pup_list_append(_ready_queue, &_thread->tnode);
    }

    PUP_LOG_DEBUG(PUP_LOG_MODULE_SCHED, "p_sched_ready_insert done:_ready_queue->head:%x", _ready_queue->next);

#if PUP_CPU_NR > 1
    if (cpuid_last != PUP_GET_CPU_ID()) {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_SCHED, "need send ipi");
        need_send = cpuid_last;
    }
    cpuid_last = (cpuid_last + 1) % PUP_CPU_NR;
#endif

    arch_spin_unlock(&_g_cpu_lock);
    arch_irq_unlock(key);

#if PUP_CPU_NR > 1
    if (need_send != CPU_NA) {
        void arch_ipi_send(pup_uint8_t cpuid);
        arch_ipi_send(need_send);
    }
#endif

    return 0;
}

struct _pthread_obj* pup_sched_ready_highest(void) {
    struct _pthread_obj* highest_thread = PUP_NULL;
    pup_base_t key = arch_irq_lock();
    arch_spin_lock(&_g_cpu_lock);

    pup_list_t* _ready_queue = &pup_cpu_self()->ready_queue;

    if (PUP_SCHED_GUARD_ENABLED && !pup_list_is_sane(_ready_queue)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SCHED, "ready_queue corrupted before pick, reinit");
        pup_list_init(_ready_queue);
        arch_spin_unlock(&_g_cpu_lock);
        arch_irq_unlock(key);
        return PUP_NULL;
    }

    if (!pup_list_is_empty(_ready_queue)) {
        highest_thread = pup_list_entry(_ready_queue->next, struct _pthread_obj, tnode);
        PUP_ASSERT(highest_thread != PUP_NULL);
    }

    arch_spin_unlock(&_g_cpu_lock);
    arch_irq_unlock(key);
    return highest_thread;
}

static int pup_sched_ready_remove(struct _pthread_obj* thread) {
    struct _pthread_obj* _thread = thread;
    pup_base_t key = arch_irq_lock();
    arch_spin_lock(&_g_cpu_lock);

    if (PUP_SCHED_GUARD_ENABLED) {
        if (!_thread || !pup_thread_ptr_valid(_thread)) {
            PUP_LOG_ERROR(PUP_LOG_MODULE_SCHED, "ready_remove invalid thread: 0x%08x", (unsigned int)_thread);
            arch_spin_unlock(&_g_cpu_lock);
            arch_irq_unlock(key);
            return -1;
        }

        if (!pup_node_is_linked(&_thread->tnode)) {
            PUP_LOG_ERROR(PUP_LOG_MODULE_SCHED, "ready_remove on unlinked node: %s", _thread->attr.name);
            arch_spin_unlock(&_g_cpu_lock);
            arch_irq_unlock(key);
            return -1;
        }

        if (!pup_list_is_sane(&pup_cpu_self()->ready_queue)) {
            PUP_LOG_ERROR(PUP_LOG_MODULE_SCHED, "ready_queue corrupted before remove, reinit");
            pup_list_init(&pup_cpu_self()->ready_queue);
            arch_spin_unlock(&_g_cpu_lock);
            arch_irq_unlock(key);
            return -1;
        }
    }

    PUP_LOG_DEBUG(PUP_LOG_MODULE_SCHED, "p_sched_ready_remove:tnode:%x",&_thread->tnode);
    pup_list_remove(&_thread->tnode);

    arch_spin_unlock(&_g_cpu_lock);

    arch_irq_unlock(key);
    return 0;
}

/**@}*/

/**
 * @addtogroup POSIX Thread
 * @{
 */

void pup_thread_entry(void (*entry)(void* parameter), void* param) {
    PUP_LOG_DEBUG(PUP_LOG_MODULE_THREAD, "p_thread_entry enter...");
    if (entry) {
        entry(param);
    }

    PUP_LOG_DEBUG(PUP_LOG_MODULE_THREAD, "p_thread_entry exit...");
    pthread_exit(PUP_NULL);
    while (1)
        ;
}
static void _p_thread_cleanup(struct _pthread_obj* obj) {
}

pthread_t pthread_self(void) {
    return pup_cpu_self()->curr_thread;
}

void* pup_thread_get_arch_data(pthread_t thread) {
    struct _pthread_obj* pthread_obj = thread;

    if (!pthread_obj) {
        return PUP_NULL;
    }

    return pthread_obj->arch_data;
}

void* pup_thread_get_stackaddr(pthread_t thread) {
    struct _pthread_obj* pthread_obj = thread;

    if (!pthread_obj) {
        return PUP_NULL;
    }

    return pthread_obj->stackaddr;
}

pup_size_t pup_thread_get_stacksize(pthread_t thread) {
    struct _pthread_obj* pthread_obj = thread;

    if (!pthread_obj) {
        return 0;
    }

    return pthread_obj->stacksize;
}

pthread_t pup_thread_next(void) {
    return pup_cpu_self()->next_thread;
}
int pthread_attr_init(pthread_attr_t* thread_attributes) {
    *thread_attributes = pthread_default_attr;
    return 0;
}
int pthread_attr_setstackaddr(pthread_attr_t* thread_attributes, void* stack_address) {
    thread_attributes->stackaddr = stack_address;
    return 0;
}
int pthread_attr_setstacksize(pthread_attr_t* thread_attributes, pup_size_t stack_size) {
    thread_attributes->stacksize = stack_size;
    return 0;
}

int pthread_attr_setpriority(pthread_attr_t* thread_attributes, int priority) {
    thread_attributes->priority = priority;
    return 0;
}
#if PUP_CPU_NR > 1
int pthread_attr_setcpu(pthread_attr_t* thread_attributes, int cpu) {
    thread_attributes->bindcpu = cpu;
    return 0;
}
#endif
/**
 * @brief set the name of a thread
 *
 * @param thread argument specifies the thread whose name is to be changed.
 * @param name name specifies the new name.
 * @return On success, these functions return 0; on error, they return a
 *             nonzero error number.
 */
int pthread_setname_np(pthread_t thread, const char* name) {
    struct _pthread_obj* pthread_obj = thread;
    pthread_obj->attr.name = name;
    return 0;
}
int pthread_attr_setname(pthread_attr_t* thread_attributes, char* name) {
    thread_attributes->name = name;
    return 0;
}
const char* pthread_getname(pthread_t thread) {
    struct _pthread_obj* pthread_obj = thread;
    if (pthread_obj) {
        return pthread_obj->attr.name;
    }
    return "(null)";
}
int pthread_create(pthread_t* thread_handle, pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg) {
    int ret = 0;
    struct _pthread_obj* pthread_obj;

    PUP_ASSERT(attr && attr->stackaddr);
    // create pthread date from stack top.
    PUP_LOG_DEBUG(PUP_LOG_MODULE_THREAD, "pthread_obj attr.stackaddr:0x%x, size:%d", attr->stackaddr, attr->stacksize);
    pthread_obj = (struct _pthread_obj*)PUP_ALIGN_DOWN((pup_ubase_t)(attr->stackaddr +
                                                                     (attr->stacksize - sizeof(struct _pthread_obj))),
                                                       PUP_ALIGN_SIZE);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_THREAD, "pthread_obj created at addr:0x%x", pthread_obj);
    if (!pthread_obj)
        return 0;

    memset(pthread_obj, 0x0, sizeof(struct _pthread_obj));

    pthread_obj->attr = *attr;

    /* initial this pthread to system */
    /* set parameter */
    pthread_obj->entry = start_routine;
    pthread_obj->param = arg;
    pthread_obj->stackaddr = pthread_obj->attr.stackaddr;
    pthread_obj->stacksize = pthread_obj->attr.stacksize - PUP_ALIGN(sizeof(struct _pthread_obj), PUP_ALIGN_SIZE);
    pthread_obj->prio = pthread_obj->attr.priority;
    pthread_obj->state = PUP_THREAD_STATE_INIT;
    pthread_obj->cleanup = _p_thread_cleanup;
    pthread_obj->exit_value = PUP_NULL;
    pthread_obj->join_thread = PUP_NULL;
#if PUP_CPU_NR > 1
    pthread_obj->bindcpu = pthread_obj->attr.bindcpu;
    pthread_obj->oncpu = CPU_NA;
#endif

    pup_list_insert(&_g_pobj_list, &pthread_obj->link_node);

    memset(pthread_obj->stackaddr, 0x23, pthread_obj->stacksize);

    pthread_obj->arch_data = arch_new_thread(pup_thread_entry, start_routine, arg, pthread_obj->stackaddr, pthread_obj->stacksize);

    if (thread_handle)
        *thread_handle = pthread_obj;
    /* start thread */
    pup_sched_ready_insert(pthread_obj);
    pup_sched();

    return ret;
}

int pthread_join(pthread_t thread_handle, void** value_destination) {
    int ret = 0;
    struct _pthread_obj* _thread = thread_handle;
    struct _pthread_obj* _self = pthread_self();
    pup_base_t key = arch_irq_lock();
    PUP_ASSERT(_thread != PUP_NULL);
    PUP_ASSERT(_thread != _self);

    if (_thread->state != PUP_THREAD_STATE_DEAD) {
        PUP_ASSERT(_thread->join_thread == PUP_NULL);
        _thread->exit_value = value_destination;
        _thread->join_thread = _self;
        _self->state = PUP_THREAD_STATE_BLOCK;
        pup_sched();
    }

    /* Reclaim thread: remove from zombie list (if placed there) and from global list */
    if (pup_node_is_linked(&_thread->tnode)) {
        pup_list_remove(&_thread->tnode);
    }
    if (pup_node_is_linked(&_thread->link_node)) {
        pup_list_remove(&_thread->link_node);
    }

    arch_irq_unlock(key);
    return ret;
}

void pthread_exit(void* exit_value) {
    struct _pthread_obj* _thread = pthread_self();
    if (_thread->exit_value) {
        *_thread->exit_value = exit_value;
    }
    pup_base_t key = arch_irq_lock();

    if (_thread->join_thread) {
        /* Has a joiner: wake it up, joiner will reclaim resources in pthread_join */
        pup_sched_ready_insert(_thread->join_thread);
        _thread->join_thread = PUP_NULL;
    } else {
        /* No joiner: place into zombie list for pup_reap_zombies() to collect */
        pup_list_append(&_g_zombie_list, &_thread->tnode);
    }
    _thread->state = PUP_THREAD_STATE_DEAD;
    pup_sched();

    arch_irq_unlock(key);
}

int pthread_cancel(pthread_t thread_handle) {
    struct _pthread_obj* _thread = thread_handle;
    struct _pthread_obj* _self = pthread_self();
    pup_base_t key;
    
    if (_thread == PUP_NULL || _thread == _self) {
        return -1;
    }
    
    key = arch_irq_lock();
    
    if (_thread->state == PUP_THREAD_STATE_DEAD) {
        arch_irq_unlock(key);
        return -1;
    }
    
    if (_thread->state == PUP_THREAD_STATE_READY) {
        pup_sched_ready_remove(_thread);
    }
    
    if (_thread->join_thread) {
        if (_thread->exit_value) {
            *_thread->exit_value = (void*)-1;
        }
        pup_sched_ready_insert(_thread->join_thread);
        _thread->join_thread = PUP_NULL;
        /* Joiner will reclaim resources in pthread_join */
    } else {
        /* No joiner: place into zombie list */
        pup_list_append(&_g_zombie_list, &_thread->tnode);
    }

    _thread->state = PUP_THREAD_STATE_DEAD;
    
    arch_irq_unlock(key);
    
    pup_sched();
    
    return 0;
}

int pthread_terminate_self(void) {
    struct _pthread_obj* _thread = pthread_self();
    pup_base_t key;
    
    if (_thread == PUP_NULL) {
        return -1;
    }
    
    key = arch_irq_lock();
    
    if (_thread->join_thread) {
        if (_thread->exit_value) {
            *_thread->exit_value = (void*)-1;
        }
        pup_sched_ready_insert(_thread->join_thread);
        _thread->join_thread = PUP_NULL;
        /* Joiner will reclaim resources in pthread_join */
    } else {
        /* No joiner: place into zombie list */
        pup_list_append(&_g_zombie_list, &_thread->tnode);
    }

    _thread->state = PUP_THREAD_STATE_DEAD;
    
    pup_sched();
    
    arch_irq_unlock(key);
    
    return 0;
}

void pup_reap_zombies(void) {
    pup_node_t* node;
    pup_node_t* node_s;
    struct _pthread_obj* thread;
    pup_base_t key = arch_irq_lock();

    pup_list_for_each_node_safe(&_g_zombie_list, node, node_s) {
        thread = pup_list_entry(node, struct _pthread_obj, tnode);
        pup_list_remove(&thread->tnode);
        if (pup_node_is_linked(&thread->link_node)) {
            pup_list_remove(&thread->link_node);
        }
    }

    arch_irq_unlock(key);
}

void pup_pthread_list(void) {
    pup_node_t* node;
    struct _pthread_obj* object;
    int maxlen;

    maxlen = 8;

    PUP_PRINTK("thread    pri  state   stack size max used\n");
    PUP_PRINTK("--------  ---  ------- ----------  ------\n");

    pup_list_for_each_node(&_g_pobj_list, node) {
        object = pup_list_entry(node, struct _pthread_obj, link_node);
        {
            pup_uint8_t stat;
            pup_uint8_t* ptr;
            struct _pthread_obj* thread = (struct _pthread_obj*)object;

            PUP_PRINTK("%-*.*s  %3d ", maxlen, maxlen, thread->attr.name, thread->prio);

            stat = thread->state;
            if (stat == PUP_THREAD_STATE_READY)
                PUP_PRINTK(" ready  ");
            else if (stat == PUP_THREAD_STATE_BLOCK)
                PUP_PRINTK(" blocked");
            else if (stat == PUP_THREAD_STATE_INIT)
                PUP_PRINTK(" init   ");
            else if (stat == PUP_THREAD_STATE_DEAD)
                PUP_PRINTK(" dead   ");
            else if (stat == PUP_THREAD_STATE_RUN)
                PUP_PRINTK(" running");
            else if (stat == PUP_THREAD_STATE_SLEEP)
                PUP_PRINTK(" sleep  ");

            ptr = (pup_uint8_t*)thread->attr.stackaddr;
            while (*ptr == '#')
                ptr++;
            PUP_PRINTK(" 0x%08x    %02d%%  \n",
                       thread->attr.stacksize,
                       (thread->attr.stacksize - ((pup_ubase_t)ptr - (pup_ubase_t)thread->attr.stackaddr)) * 100 / thread->attr.stacksize);
        }
    }
}

int sched_yield(void) {
    struct _pthread_obj* _thread = pup_cpu_self()->curr_thread;
    pup_base_t key = arch_irq_lock();

    PUP_ASSERT(pup_cpu_self()->curr_thread != PUP_NULL);
    PUP_ASSERT(_thread->state == PUP_THREAD_STATE_RUN);

    _thread->state = PUP_THREAD_STATE_INIT;
    pup_sched_ready_insert(_thread);
    pup_sched();

    arch_irq_unlock(key);
    return 0;
}

/**@}*/

void pup_show_version(void) {
    PUP_PRINTK("\n\nBuild Time: %s %s\n", __DATE__, __TIME__);
    PUP_PRINTK("                           _         \n");
    PUP_PRINTK("    ____   ____    _____  (_) _  __\n");
    PUP_PRINTK("   / __ \\ / __ \\  / ___/ / / | |/_/\n");
    PUP_PRINTK("  / /_/ // /_/ / (__  ) / /  >  <  \n");
    PUP_PRINTK(" / .___/ \\____/ /____/ /_/  /_/|_|  \n");
    PUP_PRINTK("/_/          Powered dy puppy-rtos\n");
}

char main_pthread_stack[4096];
void* puppy_main_thread(void* arg);
void puppy_init(void) {
    pthread_attr_t attr;

    kheap_init();
    pup_cpu_init();
    pup_show_version();
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, sizeof(main_pthread_stack));
    pthread_attr_setstackaddr(&attr, main_pthread_stack);
    pthread_attr_setname(&attr, "main");

    pthread_create(PUP_NULL, &attr, puppy_main_thread, PUP_NULL);
#if PUP_CPU_NR > 1
    pup_subcpu_start();
#endif
    _g_sched_runtime_ready = 1;
    pup_sched_unlock();
    while (1)
        ;
}
/**
 * @addtogroup Semaphores
 * @{
 */

int sem_init(sem_t* semaphore_handle, int pshared, unsigned int value) {
    semaphore_handle->value = value;
    pup_list_init(&semaphore_handle->blocking_list);
    return 0;
}

int sem_destroy(sem_t* semaphore_handle) {
    return 0;
}

static void _block_thread(pup_list_t* list, struct _pthread_obj* thread) {
    struct _pthread_obj* temp_thread;
    if (pup_list_is_empty(list)) {
        pup_list_append(list, &thread->tnode);
    } else {
        pup_node_t* pos = list->next;
        while (pos != list) {
            temp_thread = pup_list_entry(pos, struct _pthread_obj, tnode);
            if (temp_thread->prio > thread->prio) {
                break;
            }
            pos = pos->next;
        }
        if (pos != list) {
            pup_list_insert(pos, &thread->tnode);
        } else {
            pup_list_append(list, &thread->tnode);
        }
    }
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SCHED, "_block_thread:%s", thread->attr.name);
    thread->state = PUP_THREAD_STATE_BLOCK;
}

static void _wakeup_block_thread(pup_list_t* list) {
    struct _pthread_obj* _thread;
    _thread = pup_list_entry(list->next,
                             struct _pthread_obj, tnode);
    pup_list_remove(&_thread->tnode);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SCHED, "_wakeup_block_thread:%s", _thread->attr.name);
    pup_sched_ready_insert(_thread);
}

int sem_post(sem_t* semaphore_handle) {
    int ret = 0;
    sem_t* sem = semaphore_handle;
    pup_base_t key = arch_irq_lock();

    sem->value++;
    if (pup_list_is_empty(&sem->blocking_list)) {
        goto _exit;
    }
    _wakeup_block_thread(&sem->blocking_list);
    pup_sched();
_exit:
    arch_irq_unlock(key);
    return ret;
}

int sem_trywait(sem_t* semaphore_handle) {
    return -1;
}

int sem_wait(sem_t* semaphore_handle) {
    sem_t* sem = semaphore_handle;
    int ret = 0;
    pup_base_t key = arch_irq_lock();
    if (sem->value > 0) {
        sem->value--;
        goto _exit;
    }
    _block_thread(&sem->blocking_list, pthread_self());
    ret = pup_sched();
    if (ret == 0) {
        sem->value--;
    }
_exit:
    arch_irq_unlock(key);
    return ret;
}

/**@}*/

/**
 * @addtogroup Tick and Delay
 * @{
 */

static void _timeout_insert(struct _pthread_obj* thread)
{
    pup_node_t* node;
    struct _pthread_obj* temp_thread;

    pup_list_for_each_node(&_g_timeout_list, node)
    {
        temp_thread = pup_list_entry(node, struct _pthread_obj, tnode);
        if (temp_thread->timeout_tick > thread->timeout_tick)
        {
            PUP_LOG_DEBUG(PUP_LOG_MODULE_SCHED, "_timeout_insert: timeout_tick:%d", thread->timeout_tick);
            pup_list_insert(&temp_thread->tnode, &thread->tnode);
            return;
        }
    }
    pup_list_append(&_g_timeout_list, &thread->tnode);
}

void pup_tick_inc(void)
{
    pup_base_t key = arch_irq_lock();

    /* SysTick may fire before scheduler/runtime init completes. */
    if (!_g_sched_runtime_ready)
    {
        arch_irq_unlock(key);
        return;
    }

    _g_tick++;

    while (!pup_list_is_empty(&_g_timeout_list))
    {
        struct _pthread_obj* thread = pup_list_entry(_g_timeout_list.next, struct _pthread_obj, tnode);
        if (thread->timeout_tick > _g_tick)
        {
            break;
        }
        PUP_LOG_DEBUG(PUP_LOG_MODULE_SCHED, "_timeout_remove:%x, timeout_tick:%d", &thread, thread->timeout_tick);
        pup_list_remove(&thread->tnode);
        pup_sched_ready_insert(thread);
    }
    arch_irq_unlock(key);
    pup_sched();
}

int pthread_delay(int ticks)
{
    struct _pthread_obj* _thread = pthread_self();
    pup_base_t key = arch_irq_lock();

    if (ticks <= 0)
    {
        arch_irq_unlock(key);
        return 0;
    }

    _thread->timeout_tick = _g_tick + ticks;
    _thread->state = PUP_THREAD_STATE_SLEEP;
    _timeout_insert(_thread);

    pup_sched();
    arch_irq_unlock(key);
    return 0;
}

/**@}*/
