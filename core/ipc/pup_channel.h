#ifndef __PUP_CHANNEL_H__
#define __PUP_CHANNEL_H__

/*
 * Puppy IPC Channel + Pulse (kernel-internal API)
 *
 * A channel is a kernel message-passing endpoint. Typical usage:
 *
 *   Server (driver):
 *     id = pup_channel_create(owner_pid);
 *     while (1) {
 *         cookie = pup_channel_receive(id, &msg);
 *         if (msg.type == PUP_MSG_TYPE_PULSE) { handle_irq(); continue; }
 *         result = process_request(msg.buf);
 *         pup_channel_reply(id, cookie, result);
 *     }
 *
 *   Client (DevFS proxy or another app):
 *     result = pup_channel_send(id, shared_buf, sizeof(*shared_buf));
 *
 *   Kernel / ISR (non-blocking):
 *     pup_channel_post_pulse(id, IRQ_GPIOC, 0);
 */

#include <puppy_core.h>
#include "shared/pup_channel_abi.h"

/* Maximum number of channels in the kernel pool */
#define PUP_CHANNEL_MAX         8

/* Maximum pending messages per channel (requests + pulses combined) */
#define PUP_CHANNEL_QUEUE_DEPTH 8

/*
 * Create a channel.
 * owner_pid  – the server/driver app that will call pup_channel_receive().
 * Returns a non-negative channel_id on success, -1 on failure.
 */
int pup_channel_create(int owner_pid);

/*
 * Destroy a channel.
 * Unblocks any client threads blocked in pup_channel_send() with -1.
 * Unblocks a receiver blocked in pup_channel_receive() with -1.
 * Returns 0 on success, -1 if not found.
 */
int pup_channel_destroy(int channel_id);

/*
 * Client: enqueue a REQUEST and block until the server calls pup_channel_reply().
 * buf     – must point to shared memory (kheap_shared) if the server is user-mode.
 * buf_sz  – size of the shared payload.
 * Returns the long result supplied by pup_channel_reply(), or -1 on error.
 */
long pup_channel_send(int channel_id, void *buf, int buf_sz);

/*
 * Server: dequeue the next message; blocks when the queue is empty.
 * out     – filled with the message descriptor.
 * Returns the cookie (PUP_MSG_COOKIE_PULSE=0 for pulse; >0 for request).
 * Returns -1 if the channel is invalid or closed.
 */
int pup_channel_receive(int channel_id, pup_channel_msg_t *out);

/*
 * Server: complete a REQUEST by supplying a result value.
 * The blocked pup_channel_send() call returns this value.
 * cookie  – value previously returned by pup_channel_receive().
 * Returns 0 on success, -1 on error.
 */
int pup_channel_reply(int channel_id, int cookie, long result);

/*
 * Kernel / ISR: post a PULSE to the channel.  Non-blocking.
 * code    – event identifier (e.g. IRQ number).
 * value   – auxiliary data.
 * Returns 0 on success, -1 if the queue is full or the channel is invalid.
 */
int pup_channel_post_pulse(int channel_id, int code, int value);

/*
 * Release all channels owned by owner_pid (called on app exit).
 */
void pup_channel_cleanup_owner(int owner_pid);

/*
 * Returns 1 if channel_id is valid and open, 0 otherwise.
 */
int pup_channel_is_valid(int channel_id);

/*
 * Returns 1 if channel exists and is owned by owner_pid, 0 otherwise.
 */
int pup_channel_is_owner(int channel_id, int owner_pid);

#endif /* __PUP_CHANNEL_H__ */
