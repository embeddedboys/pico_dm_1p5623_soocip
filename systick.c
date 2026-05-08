#include "systick.h"
#include "core_cm0plus.h"

void systick_init(u8 period_ms)
{
	/* T = LOAD + 1 / fsys */
	int fsys_clk_hz = DEFAULT_SYS_CLK_KHZ * 1000;
	SysTick->LOAD = fsys_clk_hz * period_ms / 1000 - 1; /* 10ms */
	SysTick->VAL = 0;
	SysTick->CTRL = 0x07;
}
