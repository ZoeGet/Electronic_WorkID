#ifndef ST7735_H
#define ST7735_H

#include "main.h"
#include "spi.h"
#include "fonts.h"
#include "stdio.h"

#define ST7735_RST_Pin LCD_RST_Pin
#define ST7735_RST_GPIO_Port LCD_RST_GPIO_Port
#define ST7735_DC_Pin LCD_DC_Pin
#define ST7735_DC_GPIO_Port LCD_DC_GPIO_Port
#define ST7735_CS_Pin LCD_CS_Pin
#define ST7735_CS_GPIO_Port LCD_CS_GPIO_Port

////////////////////////////////////
#define ST7735_SPI_INSTANCE hspi3
////////////////////////////////////

#define ST7735_XSTART 0
#define ST7735_YSTART 0

//////////////////////////////////////////////////////////
// 宽度和高度比现实大一点，避免清屏时遗留边上像素点，实际厂商标的屏幕尺寸是 128x160
#define ST7735_WIDTH  161
#define ST7735_HEIGHT 129
//////////////////////////////////////////////////////////

// Screen Direction
#define ST7735_ROTATION 1  // 屏幕方向：0=正常，1=90度，2=180度，3=270度
// Color Mode: RGB or BGR
#define ST7735_MADCTL_RGB 0x00
#define ST7735_MADCTL_BGR 0x08
#define ST7735_MADCTL_MODE ST7735_MADCTL_RGB // 颜色模式：RGB
// Color Inverse: 0=NO, 1=YES  （是否反转颜色）
#define ST7735_INVERSE 0

// Color definitions
#define ST7735_BLACK   0x0000 // 黑色
#define ST7735_BLUE    0x001F // 蓝色
#define ST7735_RED     0xF800 // 红色
#define ST7735_GREEN   0x07E0 // 绿色
#define ST7735_CYAN    0x07FF // 青色
#define ST7735_MAGENTA 0xF81F // 紫色
#define ST7735_YELLOW  0xFFE0 // 黄色
#define ST7735_WHITE   0xFFFF // 白色

// 扩展颜色定义
// 橙色/棕色系
#define ST7735_ORANGE      0xFD20 // 橙色
#define ST7735_BROWN       0x8A00 // 棕色

// 粉色/紫色系
#define ST7735_PINK        0xF81F // 粉色
#define ST7735_PURPLE      0x780F // 紫色
#define ST7735_VIOLET      0x9192 // 紫罗兰色

// 青/蓝绿色系
#define ST7735_LIME        0x07E0 // 青柠色
#define ST7735_OLIVE       0x8400 // 橄榄色
#define ST7735_TEAL        0x0410 // 水鸭色

// 浅色系
#define ST7735_LIGHTBLUE   0x05FF // 浅蓝色
#define ST7735_LIGHTGREEN  0x07C0 // 浅绿色
#define ST7735_LIGHTYELLOW 0xFFE0 // 浅黄色
#define ST7735_LIGHTCYAN   0x07FF // 浅青色

// 深色系
#define ST7735_DARKBLUE    0x0010 // 深蓝色
#define ST7735_DARKGREEN   0x03E0 // 深绿色
#define ST7735_DARKRED     0x8000 // 深红色
#define ST7735_DARKCYAN    0x0410 // 深青色

// 灰色系
#define ST7735_GRAY        0x8410 // 灰色
#define ST7735_LIGHTGRAY   0xC618 // 浅灰色
#define ST7735_DARKGRAY    0x4208 // 深灰色

// 颜色宏定义（用于自定义 RGB565 颜色）
#define ST7735_COLOR565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

void ST7735_Init(void);
void ST7735_DrawRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void ST7735_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgColor, const FontDef *font);
void ST7735_FillScreen(uint16_t color);
void ST7735_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *image);

#endif

