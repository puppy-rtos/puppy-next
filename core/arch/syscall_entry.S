/*
 * Copyright (c) 2022-2023, The Puppy RTOS Authors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

.syntax unified
.cpu cortex-m4
.thumb
.text

.extern arch_get_current_thread
.extern syscall_handler

/*
 * void return_user_space(void);
 * This function returns to user space via svc #1
 */
.global return_user_space
.type return_user_space, %function
return_user_space:
    SVC     #1
    BX      lr

/*
 * void syscall_entry(void);
 * This function runs in Thread mode (privileged) after SVC_Handler returns.
 * It executes syscall_handler on kernel stack, then returns to user space.
 * 
 * When entering:
 *   - CPU is in Thread mode, privileged
 *   - r0-r3 contain original args (from exception frame restore)
 *   - r7 contains syscall_num
 *   - r9 contains GOT base (must be preserved!)
 *   - PSP points to exception frame
 */
.global syscall_entry
.type syscall_entry, %function
syscall_entry:
    /* save callee-saved registers (including r9 = GOT base) */
    PUSH    {r4-r11, lr}

    /* save args to callee-saved registers */
    /* r9 (GOT base) is now saved on stack, we can use it temporarily */
    MOV     r4, r7                /* r4 = syscall_num */
    MOV     r5, r0                /* r5 = arg1 */
    MOV     r6, r1                /* r6 = arg2 */
    MOV     r7, r2                /* r7 = arg3 */
    /* r3 = arg4 */

    /* get kernel stack */
    PUSH    {r3}                  /* save arg4 */
    BL      arch_get_current_thread
    LDR     r8, [r0, #4]          /* r8 = kernel_stack_ptr */
    POP     {r3}                  /* restore arg4 */

    /* switch to kernel stack */
    /* NOTE: r9 is saved on stack, we use a different approach */
    /* save user SP using r8 (already has kernel_stack_ptr, we need another reg) */
    MOV     r11, r8               /* r11 = kernel_stack_ptr (backup) */
    MRS     r8, PSP               /* r8 = user stack pointer */
    MSR     PSP, r11              /* switch to kernel stack */

    /* save user SP to kernel stack */
    PUSH    {r8}                  /* save user SP */

    /* call syscall_handler */
    MOV     r0, r4                /* arg0 = syscall_num */
    MOV     r1, r5                /* arg1 */
    MOV     r2, r6                /* arg2 */
    PUSH    {r3}                  /* arg4 via stack */
    MOV     r3, r7                /* arg3 */
    BL      syscall_handler       /* syscall_handler(num, arg1, arg2, arg3, arg4) */
    POP     {r3}                  /* restore stack */

    /* r0 = return value, save it */
    MOV     r4, r0                /* r4 = return value */

    /* restore user SP */
    POP     {r8}                  /* r8 = user SP */
    MSR     PSP, r8               /* switch back to user stack */

    /* save return value to user exception frame */
    STR     r4, [r8, #0]          /* save return value to r0 position */

    /* restore callee-saved registers (including r9 = GOT base) */
    POP     {r4-r11, lr}

    /* return to user space via SVC #1 */
    SVC     #1
    BX      lr                    /* should not reach here */

/*
 * SVC_Handler handles both SVC #0 (syscall) and SVC #1 (return to user)
 * 
 * For SVC #0 (syscall from user):
 *   - Switch to privileged Thread mode
 *   - Modify exception frame to call syscall_entry
 *   - Return to syscall_entry (Thread mode, privileged)
 * 
 * For SVC #1 (return to user from kernel):
 *   - Set CONTROL to user mode
 *   - Return to user space
 */
.global SVC_Handler
.type SVC_Handler, %function
SVC_Handler:
    TST     lr, #0x04
    ITE     EQ
    MRSEQ   r0, msp
    MRSNE   r0, psp

    /* get svc number from the instruction that caused the SVC */
    LDR     r1, [r0, #24]           /* get the PC value from stack */
    LDRB    r1, [r1, #-2]           /* get the SVC number (2 bytes before PC) */

    /* check if it's svc #0 (syscall) or svc #1 (return to user) */
    CMP     r1, #0
    BEQ     syscall_from_user
    CMP     r1, #1
    BEQ     return_to_user
    BX      lr                      /* unknown SVC number, return */

syscall_from_user:
    /* r0 = PSP (points to exception frame) */
    /* switch to privileged mode before returning to syscall_entry */
    MRS     r2, CONTROL             /* get CONTROL register */
    BIC     r2, r2, #1              /* clear bit 0 to enter privileged mode */
    MSR     CONTROL, r2             /* switch to privileged mode */
    DSB
    ISB

    /* modify exception frame to call syscall_entry */
    LDR     r2, [r0, #24]           /* get PC (points to instruction after svc) */
    ORR     r2, r2, #1              /* set Thumb bit */
    STR     r2, [r0, #20]           /* set LR = return address after syscall_entry */
    LDR     r2, =syscall_entry
    STR     r2, [r0, #24]           /* set PC = syscall_entry */
    BX      lr                      /* return to syscall_entry */

return_to_user:
    /* set CPU to user-thread mode */
    MOV     r1, #0x03               /* use PSP, user-thread mode */
    MSR     CONTROL, r1
    DSB
    ISB
    BX      lr                      /* return to user space */

.size SVC_Handler, .-SVC_Handler
.size syscall_entry, .-syscall_entry
.size return_user_space, .-return_user_space
