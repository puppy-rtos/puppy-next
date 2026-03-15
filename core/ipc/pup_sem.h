#ifndef __PUP_SEM_H__
#define __PUP_SEM_H__

#include "shared/pup_ipc.h"

/*
 * Maximum number of named semaphores in the kernel pool.
 * Adjust at build time via -DPUP_SEM_MAX=<n>.
 */
#ifndef PUP_SEM_MAX
#define PUP_SEM_MAX       8
#endif

/*
 * Maximum length (incl. NUL) of a named semaphore identifier.
 */
#ifndef PUP_SEM_NAME_MAX
#define PUP_SEM_NAME_MAX  16
#endif

/*
 * Open (or create) a named semaphore.
 *   name  – null-terminated identifier, e.g. "/my_sem"
 *   flags – combination of PUP_SEM_CREATE / PUP_SEM_EXCL
 *   value – initial counter value when creating
 * Returns a non-negative handle on success, -1 on error.
 */
int pup_sem_open(const char *name, int flags, unsigned int value);

/*
 * Decrement the reference count; destroy the semaphore when it reaches 0.
 */
int pup_sem_close(int handle);

/* Block until the semaphore value becomes > 0, then decrement it. */
int pup_sem_wait(int handle);

/* Non-blocking variant; returns -1 immediately if value == 0. */
int pup_sem_trywait(int handle);

/* Increment the semaphore value and wake one waiting thread if any. */
int pup_sem_post(int handle);

#endif /* __PUP_SEM_H__ */
