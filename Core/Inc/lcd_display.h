#ifndef __LCD_DISPLAY_H__
#define __LCD_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ff.h"
#include "recorder.h"


void LCD_DisplayInit(void);
void LCD_DisplayUploadStatus(void);
void LCD_DisplayRecordingStarted(void);
void LCD_DisplayRecordingStopped(void);
void LCD_DisplayStorageError(void);
void LCD_DisplayStorageErrorDetail(RecorderErrorStage_t stage, FRESULT fresult);
void LCD_DisplayWaiting(void);
void LCD_DisplaySoundDetected(void);
void LCD_DisplayGPS(void);    // 显示 GPS 定位信息
void LCD_DisplayWorkCardInit(void);  // 电子工牌测试界面
void LCD_DisplayClockValue(uint8_t hour, uint8_t minute);  // 大号时钟显示
void LCD_DisplayDebug(uint8_t line, const char *format, ...);  // 调试信息显示

#ifdef __cplusplus
}
#endif

#endif /* __LCD_DISPLAY_H__ */
