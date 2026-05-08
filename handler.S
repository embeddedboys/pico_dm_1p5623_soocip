.syntax unified
.thumb
.cpu cortex-m0plus

.global isr_svcall
.type isr_svcall, %function
isr_svcall:
        /* "current" points to the ready-to-run TCB. */
        @ ldr r3, =current /* the address of "current" var : r3 = &current */
        ldr r0, =current
        b switch_to

.global isr_pendsv
.type isr_pendsv, %function
isr_pendsv:
#if 0
        /* task1 sp top = 0x20000B60, PSP was set to 0x20000B40 */
        /* 
         * The hardware automatically pushed 8 regs (stack frame)
         * including (xpsr, pc, lr, r12, r3, r2, r1, r0) into PSP
         */
        mrs r0, psp     /* so PSP = 0x20000B20 */

        /* save the context of current task */
        ldr r2, =current
        ldr r1, [r2]    /* 0x20000B40 */
        subs r0, r0, #32 /* alloc space */ 
        str r0, [r1] /* update sp position to the current task's stack */

        /* 
         * save r4 ~ r11. sequence is r11, r10, r9, r8, r7, r6, r5, r4 (top->bottom)
         * In thumb mode, stmia is the only instruction to access limited
         * multiple regs (r0~r7), Therefore, in order to save r8~r11, we
         * need to call stmia twice
         */
        stmia r0!, {r4-r7}      /* 0x20000B30 */
        mov r4, r8
        mov r5, r9
        mov r6, r10
        mov r7, r11
        stmia r0!, {r4-r7} /* 0x20000B40 */

        /* get the sp position of the next task's stack */
        ldr r2, =next /* r2 = &next */
        ldr r1, [r2] /* r1 = next */
        ldr r0, [r1] /* r0 = *next : 0x20000F40 */

        /* restore the context of next task */
        adds r0, r0, #16
        ldmia r0!, {r4-r7}
        mov r8, r4
        mov r9, r5
        mov r10, r6
        mov r11, r7
        msr psp, r0

        subs r0, r0, #32
        ldmia r0!, {r4-r7}
#elif 1
        mrs r0, psp

        /* r7, r6, r5, r4, r11, r10, r9, r8 */
        subs r0, #16
        stmia r0!, {r4-r7}
        mov r4, r8
        mov r5, r9
        mov r6, r10
        mov r7, r11
        subs r0, #32
        stmia r0!, {r4-r7}
        subs r0, #16 /* sp points to the r8 */

        /* save current task sp */
        ldr r2, =current /* r2 = &next */
        ldr r1, [r2] /* r1 = next */
        str r0, [r1] /* *next = r0 */

        ldr r2, =next
        ldr r1, [r2]
        ldr r0, [r1]

        ldmia r0!, {r4-r7}
        mov r8, r4
        mov r9, r5
        mov r10, r6
        mov r11, r7
        ldmia r0!, {r4-r7}
        msr psp, r0
#else
        mrs r0, psp

        ldr r2, =current /* r2 = &next */
        ldr r1, [r2] /* r1 = next */

        subs r0, #32
        str r0, [r1] /* *next = r0 */

        /* r7, r6, r5, r4, r11, r10, r9, r8 */
        push {r4-r7}
        mov r4, r8
        mov r5, r9
        mov r6, r10
        mov r7, r11
        stmia r0!, {r4-r7}
        pop {r4-r7}
        stmia r0!, {r4-r7} /* sp points to the r8 */

        ldr r2, =next
        ldr r1, [r2]
        ldr r0, [r1]

        ldmia r0!, {r4-r7}
        mov r8, r4
        mov r9, r5
        mov r10, r6
        mov r11, r7
        ldmia r0!, {r4-r7}
        msr psp, r0
#endif
        /* hardware restore stack frame (xpsr, pc, lr, r12, r3, r2, r1, r0) from PSP */
        bx lr


.global switch_to
.type switch_to, %function
switch_to:
        ldr r1, [r0] /* r1 = current */
        ldr r0, [r1] /* r0 = *current */

        /* arm cortex-m0+ doesn't support ldmia with r8~r11 regs */
        ldmia r0!, {r4-r7}
        mov r8, r4
        mov r9, r5
        mov r10, r6
        mov r11, r7

        /* Restore the context */
        ldmia r0!, {r4-r7}

        /* restore the task stack pointer (PSP) */
        msr psp, r0
        isb /* flush pipeline */

        /* let hardware restore the stack frame */
        bx lr
