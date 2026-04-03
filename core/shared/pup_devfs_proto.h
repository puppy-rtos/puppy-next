#ifndef __PUP_DEVFS_PROTO_H__
#define __PUP_DEVFS_PROTO_H__

/*
 * DevFS proxy protocol.
 *
 * Defines operation codes and the shared request payload that the kernel
 * DevFS proxy uses to communicate with a user-mode driver via an IPC channel.
 * Both the kernel (pup_devfs.c) and user-mode driver apps include this header.
 *
 * ===== Shared Memory Passthrough Convention =====
 *
 * All cross-domain data buffers (ioctl arg, read/write buf) MUST be
 * pre-allocated in SHARED MEMORY by the caller (user app) using
 * shared_malloc(). The kernel does NOT copy or translate data --
 * it passes the pointer through to the driver as-is.
 *
 * - IOCTL:  arg must be a shared-mem pointer (or NULL).
 *           data_len is set to 0; the driver knows struct size from cmd.
 * - READ:   buf must be a shared-mem pointer.  len is passed as data_len.
 * - WRITE:  buf must be a shared-mem pointer.  len is passed as data_len.
 *
 * Only pup_devfs_proxy_payload_t itself is allocated/freed by the kernel.
 */

/* Proxy operation codes (stored in pup_devfs_proxy_payload_t.op) */
#define DEVFS_PROXY_OP_OPEN   1
#define DEVFS_PROXY_OP_CLOSE  2
#define DEVFS_PROXY_OP_READ   3
#define DEVFS_PROXY_OP_WRITE  4
#define DEVFS_PROXY_OP_IOCTL  5


/* (Legacy comment removed -- SPI TRANSFER data layout is now the caller's
 * responsibility under the shared-memory passthrough convention.) */



/*
 * Shared payload allocated in shared memory (kheap_shared) by the kernel
 * DevFS proxy.  The user-mode driver receives a pointer to this struct as
 * msg.buf in the channel message, processes it, and calls msg_reply() with
 * the integer result.
 *
 * Under the passthrough convention:
 *
 * For OPEN / CLOSE: no data_buf; driver replies 0 on success or -errno.
 *
 * For READ:  data_buf is caller's shared-memory buffer (passthrough, not
 *           kernel-allocated).  driver writes up to data_len bytes into
 *           data_buf and replies with bytes written (or -errno).
 *
 * For WRITE: data_buf is caller's shared-memory buffer (passthrough).  driver
 *           reads data_len bytes from data_buf and replies with bytes
 *           consumed (or -errno).
 *
 * For IOCTL: data_buf is caller's shared-memory buffer containing the
 *            ioctl argument structure (passthrough).  data_len is 0;
 *            the driver interprets the structure based on cmd.  Driver
 *            may read/write the structure in-place and replies with
 *            result (or -errno).
 */
typedef struct {
    int   op;        /* DEVFS_PROXY_OP_*                                       */
    int   device_id; /* logical device id set at devfs_register()              */
    int   flags;     /* OPEN: open flags                                       */
    int   cmd;       /* IOCTL: command code                                    */
    void *data_buf;  /* READ/WRITE/IOCTL: pointer to shared data buffer        */
    int   data_len;  /* requested length (READ) or data length (WRITE/IOCTL)  */
} pup_devfs_proxy_payload_t;

#endif /* __PUP_DEVFS_PROTO_H__ */
