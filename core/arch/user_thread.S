.syntax unified
.cpu cortex-m4
.thumb
.text

.global user_thread_exit
.type user_thread_exit, %function
user_thread_exit:
    mov r7, #3
    svc #0
    b .
    bx lr

.size user_thread_exit, .-user_thread_exit

.global user_app_start
.type user_app_start, %function
user_app_start:
    mov r9, r0
    mov r0, r2
    mrs r3, CONTROL
    orr r3, r3, #1
    msr CONTROL, r3
    dsb
    isb
    bx r1

.size user_app_start, .-user_app_start

.global user_thread_entry
.type user_thread_entry, %function
user_thread_entry:
    mov r9, r2
    mov r3, r0
    mov r0, r1
    mrs r2, CONTROL
    orr r2, r2, #1
    msr CONTROL, r2
    dsb
    isb
    blx r3
    mov r7, #3
    svc #0
    b .

.size user_thread_entry, .-user_thread_entry
