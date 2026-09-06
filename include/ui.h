#ifndef UI_H
#define UI_H

#include <stdint.h>

#define UI_BG     0x0B1020
#define UI_FG     0xE8ECF5
#define UI_ACCENT 0x2BD97C
#define UI_TITLEBG 0x16213E
#define UI_STATBG 0x11162B
#define UI_DIM    0x8A93B2

void ui_splash(void);
void ui_chrome(void);
void ui_status_tick(void);
void statusd(void);

#endif
