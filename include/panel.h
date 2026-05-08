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

#ifndef __PANEL_H
#define __PANEL_H

#include <stdint.h>

#define PANEL_X_RES LCD_HOR_RES
#define PANEL_Y_RES LCD_VER_RES

enum {
	LCD_ROTATE_0,
	LCD_ROTATE_90,
	LCD_ROTATE_180,
	LCD_ROTATE_270,
};
#define BIT(nr) (1UL << (nr))
#define MADCTL	0x36
#define MY	BIT(7)
#define MX	BIT(6)
#define MV	BIT(5)
#define ML	BIT(4)
#define BGR	BIT(3)
#define MH	BIT(2)
#define FH	BIT(1)
#define FV	BIT(0)

extern int panel_driver_init();
extern void panel_fill(uint16_t color);
extern void panel_video_flush(int xs, int ys, int xe, int ye, void *vmem16,
			      uint32_t len);
#endif
