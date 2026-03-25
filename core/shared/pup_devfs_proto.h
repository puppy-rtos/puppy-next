#ifndef __PUP_DEVFS_PROTO_H__
#define __PUP_DEVFS_PROTO_H__

/*
 * DevFS proxy protocol.
 *
 * Defines operation codes and the shared request payload that the kernel
 * DevFS proxy uses to communicate with a user-mode driver via an IPC channel.
 * Both the kernel (pup_devfs.c) and user-mode driver apps include this header.
 */

/* Proxy operation codes (stored in pup_devfs_proxy_payload_t.op) */
#define DEVFS_PROXY_OP_OPEN   1
#define DEVFS_PROXY_OP_CLOSE  2
#define DEVFS_PROXY_OP_READ   3
#define DEVFS_PROXY_OP_WRITE  4
#define DEVFS_PROXY_OP_IOCTL  5

/*
 * Shared payload allocated in shared memory (kheap_shared) by the kernel
 * DevFS proxy.  The user-mode driver receives a pointer to this struct as
 * msg.buf in the channel message, processes it, and calls msg_reply() with
 * the integer result.
 *
 * For READ:  driver writes up to data_len bytes into data_buf and replies
 *            with the number of bytes actually written (or -errno).
 * For WRITE: driver reads data_len bytes from data_buf and replies with the
 *            number of bytes consumed (or -errno).
 * For IOCTL: driver reads/writes data_buf (4 bytes) and replies with result.
 * For OPEN / CLOSE: no data_buf; driver replies 0 on success or -errno.
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
