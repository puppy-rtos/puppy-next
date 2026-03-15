#ifndef __PUP_IPC_H__
#define __PUP_IPC_H__

/*
 * Shared IPC constants used by both the kernel and user-mode apps.
 */

/* Flags for pup_sem_open / user_sem_open */
#define PUP_SEM_CREATE  0x01   /* Create if it does not exist */
#define PUP_SEM_EXCL    0x02   /* Fail if it already exists (requires PUP_SEM_CREATE) */

#endif /* __PUP_IPC_H__ */
