// Copyright (c) 2026 embeddedboys developers

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#include "pico/time.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/stdio_uart.h"
#include "pico/stdio_usb.h"

#include "hardware/pll.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"

#include "panel.h"
#include "backlight.h"

#include "core_cm0plus.h"
#include "systick.h"

#define pr_debug(...) printf(__VA_ARGS__)
#define __inline inline __attribute__((always_inline))

#define MAX_TASKS  2
#define STACK_SIZE 256
struct task_struct {
	u32 *sp;
};

struct task_struct tasks[MAX_TASKS];
static uint current_task = 0;
struct task_struct *current = NULL;
struct task_struct *next = NULL;

#define __WFI() asm volatile("wfi" ::: "memory")

/*
 * Disable all maskable interrupts except NMI and hardfault.
 * PendSV, SVC, SysTick, and all peripheral
 * interrupts are completely blocked.
 */
static __inline void __disable_irq(void)
{
	asm volatile("cpsid i");
}

static inline void __enable_irq(void)
{
	asm volatile("cpsie i");
}

static __inline void __ISB(void)
{
	asm volatile("isb 0xF" ::: "memory");
}

static __inline uint32_t __get_PSP(void)
{
	uint32_t result;
	asm volatile("mrs %0, psp" : "=r"(result));
	return (result);
}

static __inline void __set_PSP(uint32_t p)
{
	asm volatile("msr psp, %0" ::"r"(p) :);
}

static __inline uint32_t __get_CONTROL(void)
{
	uint32_t result;

	asm volatile("MRS %0, control" : "=r"(result));
	return (result);
}

static __inline void __set_CONTROL(uint32_t control)
{
	asm volatile("MSR control, %0" : : "r"(control) : "memory");
	__ISB();
}

void isr_hardfault(void)
{
	panic("%s\n", __func__);
}

void schedule(void)
{
	/* A simple Round Robin scheduler */
	__disable_irq();
	current = &tasks[current_task];
	current_task = (current_task + 1) % MAX_TASKS;
	next = &tasks[current_task];
	__enable_irq();

	// static uint count = 0;
	// pr_debug("%s-[%d], switch to {%d} | %08x, %08x\n", __func__, count++,
	// 	 current_task, *current, *next);

	/* This will trigger PendSV isr */
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

void isr_systick(void)
{
	schedule();
}

void start_scheduler(void)
{
	/* point `current` to the first task ready to run */
	current = &tasks[0];

	/* load the first task's sp position to the PSP */
	__set_PSP((u32)tasks[0].sp);

	/* set PSP as the active stack */
	__set_CONTROL(0x02);

	/* trigger SVC and start the first task, see `isr_svcall` */
	asm volatile("svc 0");

	for (;;)
		__WFI();
}

void task_init(struct task_struct *task, void (*entry)(void *data), u32 *sp_top,
	       void *data)
{
	u32 *sp = sp_top;

	/* r0, r1, r2, r3, r12, lr, pc, xpsr */
	*(--sp) = 0x01000000; /* Thumb mode */
	*(--sp) = (u32)entry;
	*(--sp) = 0xFFFFFFFD; /* Return to thread mode and use PSP */
	*(--sp) = 0x12; // r12
	*(--sp) = 0x03; // r3
	*(--sp) = 0x02; // r2
	*(--sp) = 0x01; // r1
	*(--sp) = (u32)data; // r0

	/* r4 ~ r11 */
	for (int i = 0; i < 8; i++) {
		// printf("%s, %p\n", __func__, sp);
		*(--sp) = 4 + i;
	}

	// dump_regs(sp);
	task->sp = sp; /* save the current pos of it's stack */
	printf("%s, %08X\n", __func__, task->sp);
}

void task1_func(void *data)
{
	panel_driver_init();

	panel_fill(0x0000);

	// sleep_ms(10);
	backlight_driver_init();
	backlight_set_level(100);
	printf("backlight set to 100%%\n");

	printf("%s, going to loop, %lld\n", data, time_us_64() / 1000);
	for (;;) {
#define CUBE_X_SIZE (LCD_HOR_RES / 3 * 2)
#define CUBE_Y_SIZE (LCD_VER_RES / 3 * 2)
		static uint16_t video_memory[CUBE_X_SIZE * CUBE_Y_SIZE] = { 0 };
		memset(video_memory, (rand() % 255), sizeof(video_memory));
		panel_video_flush(LCD_HOR_RES / 2 - (CUBE_X_SIZE / 2),
				  LCD_VER_RES / 2 - (CUBE_Y_SIZE / 2),
				  LCD_HOR_RES / 2 + (CUBE_X_SIZE / 2) - 1,
				  LCD_VER_RES / 2 + (CUBE_Y_SIZE / 2) - 1,
				  video_memory, sizeof(video_memory));
	}
}

void task2_func(void *data)
{
	int led_pin = PICO_DEFAULT_LED_PIN;

	gpio_init(led_pin);
	gpio_set_dir(led_pin, GPIO_OUT);

	for (;;) {
		gpio_put(led_pin, 1);
		sleep_ms(500);
		gpio_put(led_pin, 0);
		sleep_ms(500);
	}
}

static u32 task1_stack[STACK_SIZE] __attribute__((aligned(4)));
static u32 task2_stack[STACK_SIZE] __attribute__((aligned(4)));

int main(void)
{
/* NOTE: DO NOT MODIFY THIS BLOCK */
#define CPU_SPEED_MHZ (DEFAULT_SYS_CLK_KHZ / 1000)
	if (CPU_SPEED_MHZ > 266 && CPU_SPEED_MHZ <= 360)
		vreg_set_voltage(VREG_VOLTAGE_1_20);
	else if (CPU_SPEED_MHZ > 360 && CPU_SPEED_MHZ <= 396)
		vreg_set_voltage(VREG_VOLTAGE_1_25);
	else if (CPU_SPEED_MHZ > 396)
		vreg_set_voltage(VREG_VOLTAGE_MAX);
	else
		vreg_set_voltage(VREG_VOLTAGE_DEFAULT);

	set_sys_clock_khz(CPU_SPEED_MHZ * 1000, true);
	clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
			CPU_SPEED_MHZ * MHZ, CPU_SPEED_MHZ * MHZ);
	stdio_uart_init_full(uart0, 115200, 16, 17);
	stdio_usb_init();

	printf("\n\n\nPICO DM 1P5623 Display Template\n");

	task_init(&tasks[0], task1_func, task1_stack + STACK_SIZE, "task-1");
	task_init(&tasks[1], task2_func, task2_stack + STACK_SIZE, "task-2");

	systick_init(10);

	start_scheduler();
	return 0;
}
