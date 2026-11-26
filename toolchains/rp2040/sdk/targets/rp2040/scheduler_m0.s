    .syntax unified
    .cpu cortex-m0plus
    .thumb

    .section .text.scheduler, "ax"
    .align 2

    .global PendSV_Handler
    .type PendSV_Handler, %function
PendSV_Handler:
    cpsid i                         /* Disable interrupts during context switch */
    mrs r0, psp
    cmp r0, #0
    beq .L_no_save_context          /* If psp is 0, no running task to save */

    subs r0, #16
    stmia r0!, {r4-r7}              /* Store r4-r7 */
    mov r4, r8
    mov r5, r9
    mov r6, r10
    mov r7, r11
    subs r0, #32
    stmia r0!, {r4-r7}              /* Store r8-r11 */
    subs r0, #16

.L_no_save_context:
    /* r0 = saved SP (or NULL) */
    bl loom_scheduler_switch_context /* Returns new SP in r0 */

    /* Restore r8-r11 and r4-r7 from new task's stack */
    ldmia r0!, {r4-r7}
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    ldmia r0!, {r4-r7}

    msr psp, r0                     /* Set PSP to new task's stack */
    cpsie i                         /* Re-enable interrupts */
    ldr r0, =0xFFFFFFFD             /* EXC_RETURN to Thread mode using PSP */
    bx r0

    .global SysTick_Handler
    .type SysTick_Handler, %function
    .align 2
SysTick_Handler:
    push {lr}
    bl loom_scheduler_tick_isr
    pop {pc}

    .global loom_scheduler_trigger_pendsv
    .type loom_scheduler_trigger_pendsv, %function
    .align 2
loom_scheduler_trigger_pendsv:
    ldr r0, =0xE000ED04             /* ICSR */
    ldr r1, =0x10000000             /* PENDSVSET (bit 28) */
    str r1, [r0]
    bx lr

    .global loom_scheduler_start
    .type loom_scheduler_start, %function
    .align 2
loom_scheduler_start:
    movs r0, #0
    msr psp, r0                     /* Initialize PSP to 0 */
    bl loom_scheduler_trigger_pendsv /* Trigger initial PendSV */
    cpsie i                         /* Enable interrupts */
.L_sched_wait:
    wfi
    b .L_sched_wait
