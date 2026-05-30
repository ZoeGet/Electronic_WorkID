#ifndef __DISPLAY_CHINESE_H__
#define __DISPLAY_CHINESE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define DISPLAY_CHINESE_WIDTH    16U
#define DISPLAY_CHINESE_HEIGHT   16U
#define DISPLAY_CHINESE_BYTES    32U
#define DISPLAY_CHINESE_COUNT    25U
#define DISPLAY_CHINESE_TITLE_COUNT    8U

extern const unsigned char ling[DISPLAY_CHINESE_COUNT][DISPLAY_CHINESE_BYTES];

void DisplayChinese_DrawChar16x16(uint16_t x, uint16_t y, const unsigned char font[DISPLAY_CHINESE_BYTES], uint16_t color, uint16_t bgColor);
void DisplayChinese_DrawChar16x16Transparent(uint16_t x, uint16_t y, const unsigned char font[DISPLAY_CHINESE_BYTES], uint16_t color);
void DisplayChinese_DrawWorkCardTitle(uint16_t x, uint16_t y, uint16_t color, uint16_t bgColor);
void DisplayChinese_DrawWorkCardTitleTransparent(uint16_t x, uint16_t y, uint16_t color);
void DisplayChinese_DrawWorkCardInfo(uint16_t x, uint16_t y, uint16_t color, uint16_t bgColor);
void DisplayChinese_DrawWorkCardInfoTransparent(uint16_t x, uint16_t y, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_CHINESE_H__ */
