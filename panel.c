// Copyright (c) 2024 embeddedboys developers

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

#include "pico/time.h"
#define pr_fmt(fmt) "panel: " fmt

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#include "pico/stdio.h"
#include "pico/stdio_uart.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "panel.h"

/*
 * panel Command Table
 */

#define DRV_NAME "panel"

#define pr_debug printf

struct panel_priv;

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

struct panel_operations {
	int (*init_display)(struct panel_priv *priv);
	int (*reset)(struct panel_priv *priv);
	int (*clear)(struct panel_priv *priv, u16 clear);
	int (*blank)(struct panel_priv *priv, bool on);
	int (*sleep)(struct panel_priv *priv, bool on);
	int (*set_dir)(struct panel_priv *priv, u8 dir);
	int (*set_addr_win)(struct panel_priv *priv, int xs, int ys, int xe,
			    int ye);
	int (*set_cursor)(struct panel_priv *priv, int x, int y);
};

struct panel_display {
	u32 xres;
	u32 yres;
	u32 bpp;
	u32 rotate;
};

struct panel_priv {
	u8 *buf;

	struct {
		int reset;
		int cs; /* chip select */
		int rs; /* register/data select */
		int wr; /* write signal */
		int rd; /* read signal */
		int bl; /* backlight */
		int db[LCD_PIN_DB_COUNT];
	} gpio;

	/* device specific */
	const struct panel_operations *tftops;
	struct panel_display *display;
} g_priv;

#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof(arr[0]))
#define dm_gpio_set_value(p, v) gpio_put(p, v)
#define mdelay(v)		sleep_us(v)

extern int i80_pio_init(uint8_t db_base, uint8_t db_count, uint8_t pin_wr);
extern int i80_write_buf_rs(void *buf, size_t len, bool rs);

static void fbtft_write_gpio16_wr(struct panel_priv *priv, void *buf,
				  size_t len)
{
	u16 data;
	int i;
#ifndef DO_NOT_OPTIMIZE_FBTFT_WRITE_GPIO
	static u16 prev_data;
#endif

	/* Start writing by pulling down /WR */
	dm_gpio_set_value(priv->gpio.wr, 1);

	while (len) {
		data = *(u16 *)buf;

		/* Start writing by pulling down /WR */
		dm_gpio_set_value(priv->gpio.wr, 0);

		// printf("data : 0x%x\n", data);

		/* Set data */
#ifndef DO_NOT_OPTIMIZE_FBTFT_WRITE_GPIO
		if (data == prev_data) {
			dm_gpio_set_value(priv->gpio.wr, 1); /* used as delay */
		} else {
			for (i = 0; i < 16; i++) {
				if ((data & 1) != (prev_data & 1))
					dm_gpio_set_value(priv->gpio.db[i],
							  data & 1);
				data >>= 1;
				prev_data >>= 1;
			}
		}
#else
		for (i = 0; i < 16; i++) {
			dm_gpio_set_value(&priv->gpio.db[i], data & 1);
			data >>= 1;
		}
#endif

		/* Pullup /WR */
		dm_gpio_set_value(priv->gpio.wr, 1);

#ifndef DO_NOT_OPTIMIZE_FBTFT_WRITE_GPIO
		prev_data = *(u16 *)buf;
#endif
		buf += 2;
		len -= 2;
	}
}

static void fbtft_write_gpio16_wr_rs(struct panel_priv *priv, void *buf,
				     size_t len, bool rs)
{
	dm_gpio_set_value(priv->gpio.rs, rs);
	fbtft_write_gpio16_wr(priv, buf, len);
}

/* rs=0 means writing register, rs=1 means writing data */
#if DISP_OVER_PIO
#define write_buf_rs(p, b, l, r) i80_write_buf_rs(b, l, r)
#else
#define write_buf_rs(p, b, l, r) fbtft_write_gpio16_wr_rs(p, b, l, r)
#endif

static int panel_write_reg(struct panel_priv *priv, int len, ...)
{
	u16 *buf = (u16 *)priv->buf;
	va_list args;
	int i;

	va_start(args, len);
	*buf = (u16)va_arg(args, unsigned int);
	write_buf_rs(priv, buf, sizeof(u16), 0);
	len--;

	/* if there no privams */
	if (len == 0)
		return 0;

	for (i = 0; i < len; i++) {
		*buf = (u16)va_arg(args, unsigned int);
		buf++;
	}

	len *= 2;
	write_buf_rs(priv, priv->buf, len, 1);
	va_end(args);

	return 0;
}
#define NUMARGS(...) (sizeof((int[]){ __VA_ARGS__ }) / sizeof(int))
#define write_reg(priv, ...) \
	panel_write_reg(priv, NUMARGS(__VA_ARGS__), __VA_ARGS__)

static int panel_reset(struct panel_priv *priv)
{
	dm_gpio_set_value(priv->gpio.reset, 1);
	mdelay(10);
	dm_gpio_set_value(priv->gpio.reset, 0);
	mdelay(10);
	dm_gpio_set_value(priv->gpio.reset, 1);
	mdelay(10);
	return 0;
}

static int panel_init_display(struct panel_priv *priv)
{
	pr_debug("%s, writing initial sequence...\n", __func__);
	priv->tftops->reset(priv);
	dm_gpio_set_value(priv->gpio.rd, 1);
	mdelay(120);

	write_reg(priv, 0x11);
	mdelay(20);

	// VCI1  VCL  VGH  VGL DDVDH VREG1OUT power amplitude setting
	write_reg(priv, 0xD0, 0x07, 0x42, 0x1D);

	// VCOMH VCOM_AC amplitude setting
	write_reg(priv, 0xD1, 0x00, 0x1A, 0x09);

	// Operational Amplifier Circuit Constant Current Adjust , charge pump frequency setting
	write_reg(priv, 0xD2, 0x01, 0x22);

	// REV SM GS
	write_reg(priv, 0xC0, 0x10, 0x3B, 0x00, 0x02, 0x11);

	// Frame rate setting = 72HZ  when setting 0x03
	write_reg(priv, 0xC5, 0x03);

	// Gamma setting
	write_reg(priv, 0xC8, 0x00, 0x25, 0x21, 0x05, 0x00, 0x0A, 0x65, 0x25,
		  0x77, 0x50, 0x0F, 0x00);

	// Get_display_mode (0Dh)
	write_reg(priv, 0x0D, 0x00, 0x00);

	// LSI Test Registers
	write_reg(priv, 0xF8, 0x01);
	write_reg(priv, 0xFE, 0x00, 0x02);

	// Exit invert mode
	write_reg(priv, 0x20);

	/* 
	 * Switch Page/Column and Set BGR order
	 * As I said before, this panel is based on R61581,
	 * and according to the manual, the RGB/BGR order
	 * not supported to change, but here we can set it.
	 * I don't know why, but it works.
	 */
	write_reg(priv, 0x36, (1 << 5) | (1 << 3));

	write_reg(priv, 0x3A, 0x55);

	write_reg(priv, 0x29);
	write_reg(priv, 0x21);

	return 0;
}

static int panel_set_dir(struct panel_priv *priv, u8 dir)
{
	switch (dir) {
	case LCD_ROTATE_0:
		write_reg(priv, MADCTL, FH | BGR);
		break;
	case LCD_ROTATE_90:
		write_reg(priv, MADCTL, MV | BGR);
		break;
	case LCD_ROTATE_180:
		write_reg(priv, MADCTL, FV | BGR);
		break;
	case LCD_ROTATE_270:
		write_reg(priv, MADCTL, FV | FH | MV | BGR);
		break;
	default:
		break;
	}

	return 0;
}

static int panel_set_addr_win(struct panel_priv *priv, int xs, int ys, int xe,
			      int ye)
{
	/* set column adddress */
	write_reg(priv, 0x2A, xs >> 8, xs, xe >> 8, xe);

	/* set row address */
	write_reg(priv, 0x2B, ys >> 8, ys, ye >> 8, ye);

	/* write start */
	write_reg(priv, 0x2C);
	return 0;
}

static int panel_clear(struct panel_priv *priv, u16 clear)
{
	u32 width = priv->display->xres;
	u32 height = priv->display->yres;
	int x, y;

	pr_debug("clearing screen (%d x %d) with color 0x%x\n", width, height,
		 clear);

	priv->tftops->set_addr_win(priv, 0, 0, priv->display->xres - 1,
				   priv->display->yres - 1);

	for (x = 0; x < width; x++) {
		for (y = 0; y < height; y++) {
			write_buf_rs(priv, &clear, sizeof(u16), 1);
		}
	}

	return 0;
}

static int panel_blank(struct panel_priv *priv, bool on)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int panel_sleep(struct panel_priv *priv, bool on)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static const struct panel_operations default_panel_ops = {
	.init_display = panel_init_display,
	.reset = panel_reset,
	.clear = panel_clear,
	.blank = panel_blank,
	.sleep = panel_sleep,
	.set_dir = panel_set_dir,
	.set_addr_win = panel_set_addr_win,
};

static int panel_gpio_init(struct panel_priv *priv)
{
	printf("initializing gpios...\n");

#if DISP_OVER_PIO
	gpio_init(priv->gpio.reset);
	// gpio_init(priv->gpio.bl);
	// gpio_init(priv->gpio.cs);
	gpio_init(priv->gpio.rs);
	gpio_init(priv->gpio.rd);

	gpio_set_dir(priv->gpio.reset, GPIO_OUT);
	// gpio_set_dir(priv->gpio.bl, GPIO_OUT);
	// gpio_set_dir(priv->gpio.cs, GPIO_OUT);
	gpio_set_dir(priv->gpio.rs, GPIO_OUT);
	gpio_set_dir(priv->gpio.rd, GPIO_OUT);
#else
	int *pp = (int *)&priv->gpio;

	int len = sizeof(priv->gpio) / sizeof(priv->gpio.reset);

	while (len--) {
		gpio_init(*pp);
		gpio_set_dir(*pp, GPIO_OUT);
		pp++;
	}
#endif
	return 0;
}

static int panel_hw_init(struct panel_priv *priv)
{
	printf("initializing hardware...\n");

#if DISP_OVER_PIO
	i80_pio_init(priv->gpio.db[0], ARRAY_SIZE(priv->gpio.db),
		     priv->gpio.wr);
#endif
	panel_gpio_init(priv);

	priv->tftops->init_display(priv);
	pr_debug("%s, set dir", __func__);
	priv->tftops->set_dir(priv, priv->display->rotate);
	/* clear screen to black */
	// priv->tftops->clear(priv, 0x0);

	return 0;
}

static struct panel_display default_panel_display = {
	.xres = PANEL_X_RES,
	.yres = PANEL_Y_RES,
	.bpp = 16,
	.rotate = LCD_ROTATION,
};

static void panel_video_sync(struct panel_priv *priv, int xs, int ys, int xe,
			     int ye, void *vmem16, size_t len)
{
	// pr_debug("video sync: xs=%d, ys=%d, xe=%d, ye=%d, len=%d\n", xs, ys, xe, ye, len);
	priv->tftops->set_addr_win(priv, xs, ys, xe, ye);
	write_buf_rs(priv, vmem16, len, 1);
}

void panel_fill(uint16_t color)
{
	g_priv.tftops->clear(&g_priv, color);
}

void panel_video_flush(int xs, int ys, int xe, int ye, void *vmem16,
		       uint32_t len)
{
	panel_video_sync(&g_priv, xs, ys, xe, ye, vmem16, len);
}

/* ########### standlone ######## */
static inline void panel_write_cmd(uint16_t cmd)
{
	write_buf_rs(&g_priv, &cmd, sizeof(cmd), 0);
}
#define write_cmd panel_write_cmd
static inline void panel_write_data(uint16_t data)
{
	write_buf_rs(&g_priv, &data, sizeof(data), 1);
}
#define write_data panel_write_data

#define BUF_SIZE 64
static int panel_probe(struct panel_priv *priv)
{
	pr_debug("panel probing ...\n");

	priv->buf = (u8 *)malloc(BUF_SIZE);

	priv->display = &default_panel_display;
	priv->tftops = &default_panel_ops;

	priv->gpio.bl = LCD_PIN_BL;
	priv->gpio.reset = LCD_PIN_RST;
	priv->gpio.rd = LCD_PIN_RD;
	priv->gpio.rs = LCD_PIN_RS;
	priv->gpio.wr = LCD_PIN_WR;
	priv->gpio.cs = LCD_PIN_CS;

	/* pin0 - pin15 for I8080 data bus */
	for (int i = LCD_PIN_DB_BASE; i < ARRAY_SIZE(priv->gpio.db); i++)
		priv->gpio.db[i] = i;

	panel_hw_init(priv);

	return 0;
}

int panel_driver_init(void)
{
	panel_probe(&g_priv);
	return 0;
}
