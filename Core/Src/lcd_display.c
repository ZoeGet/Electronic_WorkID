#include "lcd_display.h"
#include "st7735.h"
#include "fonts.h"
#include "stdio.h"
#include "stdarg.h"
#include "string.h"
#include "gps.h"
#include "audio_uploader.h"
#include "display_chinese.h"

#define WORK_CARD_CLOCK_X          18U
#define WORK_CARD_CLOCK_Y          34U
#define WORK_CARD_CLOCK_DIGIT_W    24U
#define WORK_CARD_CLOCK_DIGIT_H    42U
#define WORK_CARD_CLOCK_THICKNESS  5U
#define WORK_CARD_CLOCK_GAP        3U
#define WORK_CARD_CLOCK_COLOR      ST7735_BLACK
#define WORK_CARD_CLOCK_BG_COLOR   ST7735_WHITE
#define WORK_CARD_BG_COLOR         ST7735_WHITE

static void LCD_WorkCardFillSegment(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  ST7735_FillRect(x, y, w, h, WORK_CARD_CLOCK_COLOR);
}

static void LCD_WorkCardDrawDigit(uint16_t x, uint16_t y, uint8_t digit)
{
  static const uint8_t segMap[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F
  };
  uint8_t seg;
  uint16_t t = WORK_CARD_CLOCK_THICKNESS;
  uint16_t w = WORK_CARD_CLOCK_DIGIT_W;
  uint16_t h = WORK_CARD_CLOCK_DIGIT_H;
  uint16_t midY = y + h / 2U - t / 2U;

  if (digit > 9U) {
    return;
  }

  seg = segMap[digit];

  if ((seg & 0x01U) != 0U) LCD_WorkCardFillSegment(x + t, y, w - 2U * t, t);
  if ((seg & 0x02U) != 0U) LCD_WorkCardFillSegment(x + w - t, y + t, t, h / 2U - t);
  if ((seg & 0x04U) != 0U) LCD_WorkCardFillSegment(x + w - t, midY + t, t, h / 2U - t);
  if ((seg & 0x08U) != 0U) LCD_WorkCardFillSegment(x + t, y + h - t, w - 2U * t, t);
  if ((seg & 0x10U) != 0U) LCD_WorkCardFillSegment(x, midY + t, t, h / 2U - t);
  if ((seg & 0x20U) != 0U) LCD_WorkCardFillSegment(x, y + t, t, h / 2U - t);
  if ((seg & 0x40U) != 0U) LCD_WorkCardFillSegment(x + t, midY, w - 2U * t, t);
}

static void LCD_WorkCardDrawColon(uint16_t x, uint16_t y)
{
  ST7735_FillRect(x, y + 12U, 5U, 5U, WORK_CARD_CLOCK_COLOR);
  ST7735_FillRect(x, y + 27U, 5U, 5U, WORK_CARD_CLOCK_COLOR);
}

static void LCD_WorkCardDrawDash(uint16_t x, uint16_t y)
{
  uint16_t t = WORK_CARD_CLOCK_THICKNESS;
  uint16_t w = WORK_CARD_CLOCK_DIGIT_W;
  uint16_t h = WORK_CARD_CLOCK_DIGIT_H;
  uint16_t midY = y + h / 2U - t / 2U;

  LCD_WorkCardFillSegment(x + t, midY, w - 2U * t, t);
}

void LCD_DisplayClockInvalid(void)
{
  uint16_t x = WORK_CARD_CLOCK_X;

  ST7735_FillRect(WORK_CARD_CLOCK_X - 2U, WORK_CARD_CLOCK_Y - 2U, 126U, 48U, WORK_CARD_CLOCK_BG_COLOR);

  LCD_WorkCardDrawDash(x, WORK_CARD_CLOCK_Y);
  x += WORK_CARD_CLOCK_DIGIT_W + WORK_CARD_CLOCK_GAP;

  LCD_WorkCardDrawDash(x, WORK_CARD_CLOCK_Y);
  x += WORK_CARD_CLOCK_DIGIT_W + WORK_CARD_CLOCK_GAP;

  LCD_WorkCardDrawColon(x + 2U, WORK_CARD_CLOCK_Y);
  x += 10U + WORK_CARD_CLOCK_GAP;

  LCD_WorkCardDrawDash(x, WORK_CARD_CLOCK_Y);
  x += WORK_CARD_CLOCK_DIGIT_W + WORK_CARD_CLOCK_GAP;

  LCD_WorkCardDrawDash(x, WORK_CARD_CLOCK_Y);
}

void LCD_DisplayClockValue(uint8_t hour, uint8_t minute)
{
  uint16_t x = WORK_CARD_CLOCK_X;

  hour %= 24U;
  minute %= 60U;

  ST7735_FillRect(WORK_CARD_CLOCK_X - 2U, WORK_CARD_CLOCK_Y - 2U, 126U, 48U, WORK_CARD_CLOCK_BG_COLOR);

  LCD_WorkCardDrawDigit(x, WORK_CARD_CLOCK_Y, hour / 10U);
  x += WORK_CARD_CLOCK_DIGIT_W + WORK_CARD_CLOCK_GAP;

  LCD_WorkCardDrawDigit(x, WORK_CARD_CLOCK_Y, hour % 10U);
  x += WORK_CARD_CLOCK_DIGIT_W + WORK_CARD_CLOCK_GAP;

  LCD_WorkCardDrawColon(x + 2U, WORK_CARD_CLOCK_Y);
  x += 10U + WORK_CARD_CLOCK_GAP;

  LCD_WorkCardDrawDigit(x, WORK_CARD_CLOCK_Y, minute / 10U);
  x += WORK_CARD_CLOCK_DIGIT_W + WORK_CARD_CLOCK_GAP;

  LCD_WorkCardDrawDigit(x, WORK_CARD_CLOCK_Y, minute % 10U);
}

void LCD_DisplayWorkCardInit(void)
{
  ST7735_FillScreen(WORK_CARD_BG_COLOR);
  DisplayChinese_DrawWorkCardTitle(16, 8, ST7735_BLACK, WORK_CARD_BG_COLOR);
  ST7735_DrawString(120, 22, "LOGO", ST7735_BLACK, WORK_CARD_BG_COLOR, &Font_7x10);
  LCD_DisplayClockInvalid();
  ST7735_DrawString(38, 88, "Name:XXX", ST7735_BLACK, WORK_CARD_BG_COLOR, &Font_7x10);
  ST7735_DrawString(38, 102, "Job :XXX", ST7735_BLACK, WORK_CARD_BG_COLOR, &Font_7x10);
  ST7735_DrawString(38, 116, "Dept:XXX", ST7735_BLACK, WORK_CARD_BG_COLOR, &Font_7x10);
}

static const char *LCD_GetMqttResultText(FourGMqttResult_t result)
{
  switch (result) {
    case FOUR_G_MQTT_OK:
      return "OK";
    case FOUR_G_MQTT_ERROR:
      return "ERR";
    case FOUR_G_MQTT_INVALID_PARAM:
      return "PARAM";
    case FOUR_G_MQTT_TIMEOUT:
      return "TMO";
    case FOUR_G_MQTT_BUSY:
      return "BUSY";
    case FOUR_G_MQTT_TOPIC_MISMATCH:
      return "TOPIC";
    default:
      return "UNK";
  }
}

void LCD_DisplayUploadStatus(void)
{
  char line[32];

  ST7735_FillRect(0, 0, ST7735_WIDTH, 76, ST7735_BLACK);
  ST7735_DrawString(2, 2, "WAV Upload", ST7735_GREEN, ST7735_BLACK, &Font_7x10);

  if (AudioUploader_IsBusy()) {
    ST7735_DrawString(82, 2, "BUSY", ST7735_YELLOW, ST7735_BLACK, &Font_7x10);
  } else {
    ST7735_DrawString(82, 2, "IDLE", ST7735_CYAN, ST7735_BLACK, &Font_7x10);
  }

  snprintf(line, sizeof(line), "Queue:%u Files:%lu", (unsigned int)AudioUploader_GetQueueCount(),
           (unsigned long)AudioUploader_GetUploadedFiles());
  ST7735_DrawString(2, 18, line, ST7735_WHITE, ST7735_BLACK, &Font_7x10);

  snprintf(line, sizeof(line), "Chunks:%lu Drop:%lu", (unsigned long)AudioUploader_GetUploadedChunks(),
           (unsigned long)AudioUploader_GetDroppedFiles());
  ST7735_DrawString(2, 32, line, ST7735_WHITE, ST7735_BLACK, &Font_7x10);

  switch (AudioUploader_GetLastResult()) {
    case AUDIO_UPLOADER_OK:
      ST7735_DrawString(2, 46, "Last:OK", ST7735_GREEN, ST7735_BLACK, &Font_7x10);
      break;
    case AUDIO_UPLOADER_QUEUE_FULL:
      ST7735_DrawString(2, 46, "Last:QueueFull", ST7735_RED, ST7735_BLACK, &Font_7x10);
      break;
    case AUDIO_UPLOADER_FILE_ERROR:
      ST7735_DrawString(2, 46, "Last:FileErr", ST7735_RED, ST7735_BLACK, &Font_7x10);
      break;
    case AUDIO_UPLOADER_MQTT_ERROR:
      snprintf(line, sizeof(line), "Last:MQTT %s", LCD_GetMqttResultText(AudioUploader_GetLastMqttResult()));
      ST7735_DrawString(2, 46, line, ST7735_RED, ST7735_BLACK, &Font_7x10);
      break;
    default:
      ST7735_DrawString(2, 46, "Last:OtherErr", ST7735_RED, ST7735_BLACK, &Font_7x10);
      break;
  }

  snprintf(line, sizeof(line), "O:%s F:%s", FourG_MQTT_AsyncGetErrorOpText(), FourG_MQTT_AsyncGetErrorStepText());
  ST7735_FillRect(0, 60, ST7735_WIDTH, 42, ST7735_BLACK);
  ST7735_DrawString(2, 60, line, ST7735_CYAN, ST7735_BLACK, &Font_7x10);

  {
    snprintf(line, sizeof(line), "TX:%s", FourG_MQTT_GetTxSummary());
    ST7735_DrawString(2, 74, line, ST7735_YELLOW, ST7735_BLACK, &Font_7x10);
    snprintf(line, sizeof(line), "RX:%s", FourG_MQTT_GetRxSummary());
    ST7735_DrawString(2, 88, line, ST7735_WHITE, ST7735_BLACK, &Font_7x10);
  }
}

void LCD_DisplayInit(void) // LCD 显示初始化
{
  ST7735_FillScreen(ST7735_BLACK);
  LCD_DisplayUploadStatus();
}

void LCD_DisplayRecordingStarted(void)
{
  ST7735_FillScreen(ST7735_BLACK);
  ST7735_DrawString(20, 30, "Recording Start", ST7735_GREEN, ST7735_BLACK, &Font_7x10);
}

void LCD_DisplayRecordingStopped(void)
{
  ST7735_FillScreen(ST7735_BLACK);
  ST7735_DrawString(18, 30, "Recording Stop", ST7735_YELLOW, ST7735_BLACK, &Font_7x10);
}

void LCD_DisplayStorageError(void)
{
  ST7735_FillScreen(ST7735_BLACK);
  ST7735_DrawString(35, 30, "SD ERROR", ST7735_RED, ST7735_BLACK, &Font_7x10);
}

void LCD_DisplayStorageErrorDetail(RecorderErrorStage_t stage, FRESULT fresult)
{
  char line1[20];
  char line2[20];

  ST7735_FillScreen(ST7735_BLACK);

  if ((stage == RECORDER_STAGE_MOUNT) ||
      (stage == RECORDER_STAGE_OPEN) ||
      (stage == RECORDER_STAGE_WRITE_HEADER) ||
      (stage == RECORDER_STAGE_PREALLOC) ||
      (stage == RECORDER_STAGE_WRITE_DATA) ||
      (stage == RECORDER_STAGE_SYNC) ||
      (stage == RECORDER_STAGE_REWRITE_HEADER) ||
      (stage == RECORDER_STAGE_CLOSE))
  {
    ST7735_DrawString(35, 15, "SD ERROR", ST7735_RED, ST7735_BLACK, &Font_7x10);
  }
  else
  {
    ST7735_DrawString(18, 15, "REC ERROR", ST7735_RED, ST7735_BLACK, &Font_7x10);
  }

  snprintf(line1, sizeof(line1), "STEP:%s", Recorder_GetErrorStageString(stage));
  snprintf(line2, sizeof(line2), "FR:%d", (int)fresult);

  ST7735_DrawString(5, 40, line1, ST7735_YELLOW, ST7735_BLACK, &Font_7x10);
  ST7735_DrawString(5, 55, line2, ST7735_WHITE, ST7735_BLACK, &Font_7x10);
}

/**
  * @brief  显示等待界面
  *         清空状态区域并显示等待提示
  */
void LCD_DisplayWaiting(void)
{
  // 清空上半屏（0-54 行）
  ST7735_FillRect(0, 0, 161, 70, ST7735_BLACK);
  
  // 显示标题
  ST7735_DrawString(0, 10, "Sound Detector", ST7735_GREEN, ST7735_BLACK, &Font_11x18); 

  // 显示采样率
  ST7735_DrawString(0, 30, "16kHz Sampling", ST7735_CYAN, ST7735_BLACK, &Font_7x10);
  
  // 显示等待状态
  ST7735_DrawString(3, 40, "Waiting...", ST7735_YELLOW, ST7735_BLACK, &Font_7x10);
}

/**
  * @brief  显示声音检测提示
  *         清空状态区域并显示红色警告 SoundDetector_SetThreshold
  */
void LCD_DisplaySoundDetected(void)
{
  // 清空上半屏（0-54 行）
  ST7735_FillRect(0, 0, 161, 70, ST7735_BLACK);
  
  // 显示检测提示（红色，大字体）
  ST7735_DrawString(0, 25, "Sound Detected!", ST7735_RED, ST7735_BLACK, &Font_11x18);
}


/*-----------------------------------------------*/
/* GPS定位信息显示函数 */
/*-----------------------------------------------*/

/**
  * @brief  显示 GPS 定位信息
  *         在 LCD 屏幕底部显示经纬度、时间、定位状态（75-135 行）
  *         只清除 GPS 显示区域，不影响上半部分的声音检测显示
  */
void LCD_DisplayGPS(void)
{
  char displayBuffer[32];
  float latitude, longitude;
  int32_t latInteger, latFraction;
  int32_t lonInteger, lonFraction;
  
  ST7735_FillRect(0, 82, ST7735_WIDTH, ST7735_HEIGHT - 82, ST7735_BLACK);
  ST7735_DrawString(2, 84, "GPS:", ST7735_GREEN, ST7735_BLACK, &Font_7x10);
  
  if (Save_Data.isUsefull)
  {
    ST7735_DrawString(32, 84, "OK", ST7735_GREEN, ST7735_BLACK, &Font_7x10);
    
    latitude = ConvertLatitude(Save_Data.latitude, Save_Data.N_S[0]);
    latInteger = (int32_t)latitude;
    latFraction = (int32_t)((latitude - (float)latInteger) * 1000000.0f);
    if (latFraction < 0)
    {
      latFraction = -latFraction;
    }
    snprintf(displayBuffer, sizeof(displayBuffer), "LAT:%ld.%05ld", (long)latInteger, (long)(latFraction / 10));
    ST7735_DrawString(2, 96, displayBuffer, ST7735_WHITE, ST7735_BLACK, &Font_7x10);
    
    longitude = ConvertLongitude(Save_Data.longitude, Save_Data.E_W[0]);
    lonInteger = (int32_t)longitude;
    lonFraction = (int32_t)((longitude - (float)lonInteger) * 1000000.0f);
    if (lonFraction < 0)
    {
      lonFraction = -lonFraction;
    }
    snprintf(displayBuffer, sizeof(displayBuffer), "LON:%ld.%05ld", (long)lonInteger, (long)(lonFraction / 10));
    ST7735_DrawString(2, 108, displayBuffer, ST7735_WHITE, ST7735_BLACK, &Font_7x10);
    
    if (strlen(Save_Data.UTCTime) >= 6)
    {
      snprintf(displayBuffer, sizeof(displayBuffer), "UTC:%c%c:%c%c:%c%c", 
              Save_Data.UTCTime[0], Save_Data.UTCTime[1],
              Save_Data.UTCTime[2], Save_Data.UTCTime[3],
              Save_Data.UTCTime[4], Save_Data.UTCTime[5]);
      ST7735_DrawString(2, 120, displayBuffer, ST7735_CYAN, ST7735_BLACK, &Font_7x10);
    }
  }
  else
  {
    ST7735_DrawString(32, 84, "NO FIX", ST7735_YELLOW, ST7735_BLACK, &Font_7x10);
    ST7735_DrawString(2, 96, "Searching GPS", ST7735_YELLOW, ST7735_BLACK, &Font_7x10);
    ST7735_DrawString(2, 108, "Waiting fix", ST7735_YELLOW, ST7735_BLACK, &Font_7x10);
  }
}

void LCD_DisplayDebug(uint8_t line, const char *format, ...)
{
  char buffer[64];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  uint8_t y = 66U + line * 10U;
  ST7735_DrawString(5, y, buffer, ST7735_CYAN, ST7735_BLACK, &Font_7x10);
}
