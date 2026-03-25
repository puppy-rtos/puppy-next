#ifndef __PUP_CHANNEL_ABI_H__
#define __PUP_CHANNEL_ABI_H__

/*
 * Channel IPC – shared message descriptor.
 *
 * Uses only standard C types (no kernel-specific typedefs) so this header
 * can be included by both the kernel and user-mode driver apps.
 */

/* Message types */
#define PUP_MSG_TYPE_REQUEST  1  /* Client request; driver must call msg_reply()  */
#define PUP_MSG_TYPE_PULSE    2  /* Kernel/IRQ notification; no reply expected    */

/* Cookie value returned by pup_channel_receive() for a pulse */
#define PUP_MSG_COOKIE_PULSE  0

typedef struct {
    int   type;         /* PUP_MSG_TYPE_REQUEST or PUP_MSG_TYPE_PULSE           */
    int   cookie;       /* non-zero slot handle for REQUEST; 0 for PULSE        */
    void *buf;          /* shared-memory payload pointer (REQUEST) or NULL      */
    int   buf_sz;       /* payload size in bytes                                */
    int   pulse_code;   /* for PULSE: event code (e.g. IRQ number)              */
    int   pulse_value;  /* for PULSE: auxiliary value                           */
    int   sender_pid;   /* originating app pid (0 = kernel)                    */
} pup_channel_msg_t;

#endif /* __PUP_CHANNEL_ABI_H__ */
