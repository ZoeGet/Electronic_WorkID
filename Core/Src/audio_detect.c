#include "audio_detect.h"
#include "adc.h"
#include "tim.h"
#include "led.h"
#include <string.h>

/* USER CODE BEGIN 0 */

/* 全局声音检测器实例 */
SoundDetector_t gSoundDetector;

/* 外部变量引用 */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

/**
  * @brief  初始化声音检测器
  * @param  det: 声音检测器结构体指针
  * @param  buf1: DMA 双缓冲 1 指针
  * @param  buf2: DMA 双缓冲 2 指针
  * @param  size: 缓冲区大小
  * @retval 无
  */
void SoundDetector_Init(SoundDetector_t* det, uint16_t* buf1, uint16_t* buf2, uint16_t size)
{
  memset(det, 0, sizeof(SoundDetector_t));
  
  det->buffer1 = buf1;
  det->buffer2 = buf2;
  det->bufferSize = size;
  det->bufferReady = 0;
  det->threshold = SOUND_THRESHOLD;
  det->soundDetected = false;
  det->detectTime = 0;
  det->lastStdDev = 0;
  det->detectCount = 0;
}

/**
  * @brief  计算标准差（信号能量）
  * @param  buffer: ADC 数据缓冲区
  * @param  size: 缓冲区大小
  * @param  avg: 平均值指针（可选）
  * @retval 标准差值
  */
static uint32_t SoundDetector_CalculateStdDev(uint16_t* buffer, uint16_t size, uint32_t* avg)
{
  uint32_t sum = 0;
  
  /* 计算平均值 */
  for (uint16_t i = 0; i < size; i++) {
    sum += buffer[i];
  }
  uint32_t mean = sum / size;
  
  if (avg) {
    *avg = mean;
  }
  
  /* 计算方差 */
  uint32_t variance_sum = 0;
  for (uint16_t i = 0; i < size; i++) {
    int32_t diff = (int32_t)buffer[i] - (int32_t)mean;
    variance_sum += (uint32_t)(diff * diff);
  }
  uint32_t variance = variance_sum / size;
  
  /* 快速近似平方根 */
  uint32_t std_dev = 0;
  uint32_t temp = variance;
  while (temp > 0) {
    std_dev++;
    temp >>= 2;
  }
  
  return std_dev;
}

/**
  * @brief  处理 DMA 缓冲区数据
  * @param  det: 声音检测器结构体指针
  * @retval 是否检测到声音
  */
bool SoundDetector_Process(SoundDetector_t* det)
{
  if (det->bufferReady == 0) {
    return false;
  }
  
  /* 选择当前缓冲区 */
  uint16_t* currentBuffer = (det->bufferReady == 1) ? det->buffer1 : det->buffer2;
  
  /* 计算标准差 */
  uint32_t stdDev = SoundDetector_CalculateStdDev(currentBuffer, det->bufferSize / 2, NULL);
  det->lastStdDev = stdDev;
  
  /* 声音检测 */
  if (stdDev > det->threshold && !det->soundDetected) {
    det->soundDetected = true;
    det->detectTime = HAL_GetTick();
    det->detectCount++;  // 递增检测次数
  }
  
  /* 清除缓冲区标志 */
  det->bufferReady = 0;
  
  return det->soundDetected;
}

/**
  * @brief  设置检测阈值
  * @param  det: 声音检测器结构体指针
  * @param  threshold: 阈值
  * @retval 无
  */
void SoundDetector_SetThreshold(SoundDetector_t* det, uint32_t threshold)
{
  det->threshold = threshold;
}

/**
  * @brief  重置声音检测器
  * @param  det: 声音检测器结构体指针
  * @retval 无
  */
void SoundDetector_Reset(SoundDetector_t* det)
{
  det->soundDetected = false;
  det->detectTime = 0;
  det->lastStdDev = 0;
}

/**
  * @brief  检查是否检测到声音
  * @param  det: 声音检测器结构体指针
  * @retval 是否检测到声音
  */
bool SoundDetector_IsDetected(SoundDetector_t* det)
{
  if (!det->soundDetected) {
    return false;
  }
  
  /* 检查显示超时 */
  if ((HAL_GetTick() - det->detectTime) >= SOUND_DISPLAY_TIME) {
    det->soundDetected = false;
    return false;
  }
  
  return true;
}

/**
  * @brief  获取标准差值
  * @param  det: 声音检测器结构体指针
  * @retval 标准差值
  */
uint32_t SoundDetector_GetStdDev(SoundDetector_t* det)
{
  return det->lastStdDev;
}

/**
  * @brief  DMA 半传输完成回调
  * @param  hadc: ADC 句柄
  */
void SoundDetector_OnAdcHalfComplete(SoundDetector_t* det)
{
  if (det != NULL) {
    det->bufferReady = 1;  // 缓冲区 1 就绪
  }
}

/**
  * @brief  DMA 全传输完成回调
  * @param  hadc: ADC 句柄
  */
void SoundDetector_OnAdcFullComplete(SoundDetector_t* det)
{
  if (det != NULL) {
    det->bufferReady = 2;  // 缓冲区 2 就绪
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
