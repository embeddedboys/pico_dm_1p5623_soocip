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

	panel_driver_init();

	panel_fill(0x0000);

	sleep_ms(10);
	backlight_driver_init();
	backlight_set_level(100);
	printf("backlight set to 100%%\n");

	printf("going to loop, %lld\n", time_us_64() / 1000);
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

		// tight_loop_contents();
		// sleep_ms(200);
	}

	return 0;
}
