#ifndef MENU_MAIN_SCREEN_H
#define MENU_MAIN_SCREEN_H

#include <stdint.h>

#define SCREEN_SAVER_TIMEOUT_MS 30000U // 30 seconds idle trigger
#define SCREEN_SAVER_SCROLL_MS  250U   // 250 ms per character shift
#define SCREEN_SAVER_ROW0_TEXT  "SWR & Power Mtr "

// Render the primary RF measurement screen
void renderMainScreen(void);

// Render power bar into a 16-character buffer
void renderPowerBar(char *outBuf16, uint16_t currentW, uint16_t maxW, uint8_t style);

// Refresh the standby running marquee buffer text
void updateRunningText(void);

#endif // MENU_MAIN_SCREEN_H
