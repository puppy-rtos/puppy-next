.syntax unified
.cpu cortex-m4
.thumb

.text

.global syscall
.type syscall, %function
syscall:
    push {r4, r5, r6, r7, lr}
    mov r7, r0
    mov r0, r1
    mov r1, r2
    mov r2, r3
    ldr r3, [sp, #20]
    svc #0
    pop {r4, r5, r6, r7, pc}
