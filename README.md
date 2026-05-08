# Pico_DM_1P5623 Template

This is a template project that you can use to port various GUI libraries, such as lvgl.

## Getting started

Here is an example showing how to invoke the display function.

```c
#include "panel.h"

    panel_driver_init();

    /* void panel_fill(uint16_t color); */
    panel_fill(0x0000);

#define CUBE_X_SIZE (LCD_HOR_RES / 3 * 2)
#define CUBE_Y_SIZE (LCD_VER_RES / 3 * 2)
    static uint16_t video_memory[CUBE_X_SIZE * CUBE_Y_SIZE] = {0};
    memset(video_memory, (rand() % 255), sizeof(video_memory));

    /* void panel_video_flush(int xs, int ys, int xe, int ye, void *vmem16, uint32_t len); */
    panel_video_flush(
        LCD_HOR_RES / 2 - (CUBE_X_SIZE / 2),
        LCD_VER_RES / 2 - (CUBE_Y_SIZE / 2),
        LCD_HOR_RES / 2 + (CUBE_X_SIZE / 2) - 1,
        LCD_VER_RES / 2 + (CUBE_Y_SIZE / 2) - 1,
        video_memory, sizeof(video_memory)
    );
```

### Technical specifications
| Part | Model |
| --- | --- |
| Core Board | Rasberrypi Pico |
| Display | 3.5' 480x320 panel no IPS |
| | 16-bit I8080 @18MHz |
| TouchScreen | None |

### Pinout

| Left | Right |
| --- | --- |
| GP0/DB0 | VBUS |
| GP1/DB1 | VSYS |
| GND | GND |
| GP2/DB2 | 3V3_EN |
| ... | ... |

#### panel Display pins

- GP0 ~ GP15 -> panel DB0-DB15 16 pins

- GP18 -> panel CS (Chip select)

- GP19 -> panel WR (write signal)

- GP20 -> panel RS (Register select, Active Low, 0: cmd, 1: data)

- GP22 -> panel Reset (Active Low)

- GP28 -> panel Backlight (Active High)
