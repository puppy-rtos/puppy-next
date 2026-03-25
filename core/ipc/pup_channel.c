#include "pup_channel.h"
#include "puppy_log.h"
#include <libcm.h>

/* -----------------------------------------------------------------------
 * Internal types
 * --------------------------------------------------------------------- */

typedef struct {
    sem_t sem;      /* sender waits here until reply */
    long  result;   /* reply value written by server */
    int   used;     /* 1 while a request is pending  */
} pup_ch_slot_t;

typedef struct {
    int               used;
    int               owner_pid;
    int               closed;
    sem_t             recv_sem;                         /* receiver blocks here */
    pup_channel_msg_t queue[PUP_CHANNEL_QUEUE_DEPTH];   /* message ring buffer  */
    int               q_head;
    int               q_tail;
    int               q_count;
    pup_ch_slot_t     slots[PUP_CHANNEL_QUEUE_DEPTH];   /* per-request reply    */
} pup_channel_t;

static pup_channel_t g_channels[PUP_CHANNEL_MAX];
static int           g_initialized = 0;

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

static void channels_init_once(void)
{
    if (g_initialized) {
        return;
    }
    libcm_memset(g_channels, 0, sizeof(g_channels));
    g_initialized = 1;
}

static pup_channel_t *ch_get(int id)
{
    if (id < 0 || id >= PUP_CHANNEL_MAX) {
        return PUP_NULL;
    }
    if (!g_channels[id].used || g_channels[id].closed) {
        return PUP_NULL;
    }
    return &g_channels[id];
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

int pup_channel_create(int owner_pid)
{
    int i;
    int j;
    pup_base_t key;

    channels_init_once();

    key = arch_irq_lock();
    for (i = 0; i < PUP_CHANNEL_MAX; i++) {
        /* Re-use slots that were destroyed (closed=1) or never used (used=0) */
        if (!g_channels[i].used || g_channels[i].closed) {
            pup_channel_t *ch = &g_channels[i];
            libcm_memset(ch, 0, sizeof(*ch));
            ch->used      = 1;
            ch->owner_pid = owner_pid;
            ch->closed    = 0;
            arch_irq_unlock(key);

            sem_init(&ch->recv_sem, 0, 0);
            for (j = 0; j < PUP_CHANNEL_QUEUE_DEPTH; j++) {
                sem_init(&ch->slots[j].sem, 0, 0);
            }

            PUP_LOG_INFO(PUP_LOG_MODULE_IPC,
                         "channel_create: id=%d owner=%d", i, owner_pid);
            return i;
        }
    }
    arch_irq_unlock(key);
    PUP_LOG_ERROR(PUP_LOG_MODULE_IPC, "channel_create: pool exhausted");
    return -1;
}

int pup_channel_destroy(int channel_id)
{
    pup_channel_t *ch;
    pup_base_t     key;
    int            j;
    int            wake[PUP_CHANNEL_QUEUE_DEPTH];
    int            nwake = 0;

    channels_init_once();

    key = arch_irq_lock();
    if (channel_id < 0 || channel_id >= PUP_CHANNEL_MAX) {
        arch_irq_unlock(key);
        return -1;
    }
    ch = &g_channels[channel_id];
    if (!ch->used) {
        arch_irq_unlock(key);
        return -1;
    }

    ch->closed = 1;

    /* Collect slots that have blocked senders */
    for (j = 0; j < PUP_CHANNEL_QUEUE_DEPTH; j++) {
        if (ch->slots[j].used) {
            ch->slots[j].result = -1;
            wake[nwake++]       = j;
        }
    }
    arch_irq_unlock(key);

    /* Wake blocked senders (outside lock because sem_post re-acquires irq lock) */
    for (j = 0; j < nwake; j++) {
        sem_post(&ch->slots[wake[j]].sem);
    }

    /* Wake a blocked receiver */
    sem_post(&ch->recv_sem);

    PUP_LOG_INFO(PUP_LOG_MODULE_IPC, "channel_destroy: id=%d", channel_id);
    return 0;
}

long pup_channel_send(int channel_id, void *buf, int buf_sz)
{
    pup_channel_t *ch;
    pup_base_t     key;
    int            slot = -1;
    int            i;
    long           result;

    channels_init_once();

    key = arch_irq_lock();
    ch  = ch_get(channel_id);
    if (!ch) {
        arch_irq_unlock(key);
        return -1;
    }

    if (ch->q_count >= PUP_CHANNEL_QUEUE_DEPTH) {
        arch_irq_unlock(key);
        PUP_LOG_WARNING(PUP_LOG_MODULE_IPC,
                        "channel_send: queue full id=%d", channel_id);
        return -1;
    }

    /* Allocate a reply slot */
    for (i = 0; i < PUP_CHANNEL_QUEUE_DEPTH; i++) {
        if (!ch->slots[i].used) {
            slot                = i;
            ch->slots[i].used   = 1;
            ch->slots[i].result = 0;
            break;
        }
    }
    if (slot < 0) {
        arch_irq_unlock(key);
        PUP_LOG_WARNING(PUP_LOG_MODULE_IPC,
                        "channel_send: no free slot id=%d", channel_id);
        return -1;
    }

    /* Enqueue the request message */
    {
        pup_channel_msg_t *msg = &ch->queue[ch->q_tail];
        msg->type        = PUP_MSG_TYPE_REQUEST;
        msg->cookie      = slot + 1;   /* 0 is reserved for pulse */
        msg->buf         = buf;
        msg->buf_sz      = buf_sz;
        msg->pulse_code  = 0;
        msg->pulse_value = 0;
        msg->sender_pid  = 0;          /* kernel-side send; caller may patch this */
    }
    ch->q_tail  = (ch->q_tail + 1) % PUP_CHANNEL_QUEUE_DEPTH;
    ch->q_count++;
    arch_irq_unlock(key);

    /* Wake the server */
    sem_post(&ch->recv_sem);

    /* Block until the server calls pup_channel_reply() */
    sem_wait(&ch->slots[slot].sem);

    result = ch->slots[slot].result;

    key = arch_irq_lock();
    ch->slots[slot].used = 0;
    arch_irq_unlock(key);

    return result;
}

int pup_channel_receive(int channel_id, pup_channel_msg_t *out)
{
    pup_channel_t *ch;
    pup_base_t     key;

    channels_init_once();

    if (channel_id < 0 || channel_id >= PUP_CHANNEL_MAX) {
        return -1;
    }
    key = arch_irq_lock();
    ch = &g_channels[channel_id];
    if (!ch->used) {
        arch_irq_unlock(key);
        return -1;
    }
    arch_irq_unlock(key);

    /* Block until a message arrives or the channel is closed */
    if (sem_wait(&ch->recv_sem) != 0) {
        return -1;
    }

    key = arch_irq_lock();
    /* Re-check: channel might have been destroyed while we waited */
    if (!ch->used || (ch->closed && ch->q_count == 0)) {
        arch_irq_unlock(key);
        return -1;
    }

    *out = ch->queue[ch->q_head];
    ch->q_head  = (ch->q_head + 1) % PUP_CHANNEL_QUEUE_DEPTH;
    ch->q_count--;
    arch_irq_unlock(key);

    return out->cookie;
}

int pup_channel_reply(int channel_id, int cookie, long result)
{
    pup_channel_t *ch;
    pup_base_t     key;
    int            slot;

    channels_init_once();

    if (cookie <= 0 || cookie > PUP_CHANNEL_QUEUE_DEPTH) {
        return -1;
    }
    slot = cookie - 1;

    key = arch_irq_lock();
    if (channel_id < 0 || channel_id >= PUP_CHANNEL_MAX) {
        arch_irq_unlock(key);
        return -1;
    }
    ch = &g_channels[channel_id];
    if (!ch->used || !ch->slots[slot].used) {
        arch_irq_unlock(key);
        return -1;
    }

    ch->slots[slot].result = result;
    arch_irq_unlock(key);

    sem_post(&ch->slots[slot].sem);
    return 0;
}

int pup_channel_post_pulse(int channel_id, int code, int value)
{
    pup_channel_t *ch;
    pup_base_t     key;

    channels_init_once();

    key = arch_irq_lock();
    if (channel_id < 0 || channel_id >= PUP_CHANNEL_MAX) {
        arch_irq_unlock(key);
        return -1;
    }
    ch = &g_channels[channel_id];
    if (!ch->used || ch->closed || ch->q_count >= PUP_CHANNEL_QUEUE_DEPTH) {
        arch_irq_unlock(key);
        return -1;
    }

    {
        pup_channel_msg_t *msg = &ch->queue[ch->q_tail];
        msg->type        = PUP_MSG_TYPE_PULSE;
        msg->cookie      = PUP_MSG_COOKIE_PULSE;
        msg->buf         = PUP_NULL;
        msg->buf_sz      = 0;
        msg->sender_pid  = 0;
        msg->pulse_code  = code;
        msg->pulse_value = value;
    }
    ch->q_tail  = (ch->q_tail + 1) % PUP_CHANNEL_QUEUE_DEPTH;
    ch->q_count++;
    arch_irq_unlock(key);

    sem_post(&ch->recv_sem);
    return 0;
}

void pup_channel_cleanup_owner(int owner_pid)
{
    int i;

    channels_init_once();

    for (i = 0; i < PUP_CHANNEL_MAX; i++) {
        if (g_channels[i].used && !g_channels[i].closed
            && g_channels[i].owner_pid == owner_pid) {
            PUP_LOG_INFO(PUP_LOG_MODULE_IPC,
                         "channel_cleanup: id=%d owner=%d", i, owner_pid);
            pup_channel_destroy(i);
        }
    }
}

int pup_channel_is_valid(int channel_id)
{
    channels_init_once();
    if (channel_id < 0 || channel_id >= PUP_CHANNEL_MAX) {
        return 0;
    }
    return g_channels[channel_id].used && !g_channels[channel_id].closed;
}

int pup_channel_is_owner(int channel_id, int owner_pid)
{
    channels_init_once();
    if (channel_id < 0 || channel_id >= PUP_CHANNEL_MAX || owner_pid <= 0) {
        return 0;
    }
    return g_channels[channel_id].used
        && !g_channels[channel_id].closed
        && g_channels[channel_id].owner_pid == owner_pid;
}
