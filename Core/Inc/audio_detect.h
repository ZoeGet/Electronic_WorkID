#ifndef __AUDIO_DETECT_H__
#define __AUDIO_DETECT_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* 声音检测器配置 */
#define SOUND_BUFFER_SIZE       128U    // DMA 缓冲区大小
#define SOUND_THRESHOLD         10U     // 声音检测阈值（标准差）
#define SOUND_DISPLAY_TIME    1000U     // 显示持续时间（ms）

/* 声音检测器状态 */
typedef struct {
  uint16_t* buffer1;          // DMA 双缓冲 1
  uint16_t* buffer2;          // DMA 双缓冲 2
  uint16_t bufferSize;        // 缓冲区大小
  volatile uint8_t bufferReady;  // 缓冲区就绪标志
  uint32_t threshold;         // 检测阈值
  bool soundDetected;         // 声音检测标志
  uint32_t detectTime;        // 检测时间
  uint32_t lastStdDev;        // 上次标准差值
  uint32_t detectCount;       // 检测次数统计
} SoundDetector_t;

/* 外部变量 --------------------------------------------------------------*/
extern SoundDetector_t gSoundDetector;
extern uint16_t adc_buffer1[];
extern uint16_t adc_buffer2[];

/* 导出函数原型 -------------------------------------------------------------*/
void SoundDetector_Init(SoundDetector_t* det, uint16_t* buf1, uint16_t* buf2, uint16_t size);
bool SoundDetector_Process(SoundDetector_t* det);
void SoundDetector_SetThreshold(SoundDetector_t* det, uint32_t threshold);
void SoundDetector_Reset(SoundDetector_t* det);
bool SoundDetector_IsDetected(SoundDetector_t* det);
uint32_t SoundDetector_GetStdDev(SoundDetector_t* det);
void SoundDetector_OnAdcHalfComplete(SoundDetector_t* det);
void SoundDetector_OnAdcFullComplete(SoundDetector_t* det);

#ifdef __cplusplus
}
#endif

#endif /* __SOUND_DETECTOR_H__ */
