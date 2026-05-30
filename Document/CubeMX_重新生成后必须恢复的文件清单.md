# CubeMX 重新生成后必须恢复的文件清单

本文档用于当前项目 `Electronic_WorkID` 在重新生成 `CubeMX` 代码后，快速恢复录音系统相关修改。

适用范围：
- 芯片：`STM32L431RCT6`
- 录音输入：`MAX9814 -> PC0 (ADC1_IN1)`
- 存储：`SDMMC1 + FATFS`
- 按键：`KEY0 -> PC13`，`KEY1 -> PA0`

---

# 1. 每次 CubeMX 重新生成后，优先检查这些文件

高风险会被覆盖的文件：

- `Core/Src/main.c`
- `Core/Src/gpio.c`
- `Core/Src/adc.c`
- `Core/Src/tim.c`
- `Core/Src/sdmmc.c`
- `Core/Src/stm32l4xx_it.c`
- `FATFS/Target/sd_diskio.c`
- `cmake/stm32cubemx/CMakeLists.txt`

通常不会被 CubeMX 直接覆盖，但需要保证已参与编译的文件：

- `Core/Src/audio_buffer.c`
- `Core/Src/recorder.c`
- `Core/Src/wav_format.c`
- `Core/Src/lcd_display.c`
- `Core/Src/key.c`
- `Core/Src/audio_detect.c`
- `Core/Src/st7735.c`
- `Core/Src/fonts.c`
- `Core/Src/led.c`
- `Core/Src/gps.c`

---

# 2. 必须恢复的关键配置

## 2.1 `Core/Src/gpio.c`

按键原理图为“按下接地”，因此必须恢复为内部上拉输入：

- `PC13 -> GPIO_PULLUP`
- `PA0 -> GPIO_PULLUP`

正确配置应为：

```c
/*Configure GPIO pin : PC13 */
GPIO_InitStruct.Pin = GPIO_PIN_13;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;
HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

/*Configure GPIO pin : PA0 */
GPIO_InitStruct.Pin = GPIO_PIN_0;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
```

如果 CubeMX 生成成 `GPIO_NOPULL`，需要手动改回。

---

## 2.2 `Core/Src/sdmmc.c`

### SDMMC1 初始化参数必须恢复

必须使用较低初始化时钟：

```c
hsd1.Init.ClockDiv = 118;
```

不要被改回：

```c
hsd1.Init.ClockDiv = 0;
```

### SDMMC1 GPIO 上下拉必须恢复

- `PC8 (D0) -> GPIO_PULLUP`
- `PD2 (CMD) -> GPIO_PULLUP`
- `PC12 (CK) -> GPIO_NOPULL`

正确配置片段：

```c
GPIO_InitStruct.Pin = GPIO_PIN_8;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull = GPIO_PULLUP;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

GPIO_InitStruct.Pin = GPIO_PIN_12;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

GPIO_InitStruct.Pin = GPIO_PIN_2;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull = GPIO_PULLUP;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
```

---

## 2.3 `FATFS/Target/sd_diskio.c`

这是最关键、最容易被覆盖导致“按下 KEY0 后卡死”的文件。

本项目当前必须保留：

- `SD_TIMEOUT = 1000`
- 使用阻塞式轮询读写：
  - `BSP_SD_ReadBlocks(...)`
  - `BSP_SD_WriteBlocks(...)`
- 不使用 CubeMX 默认生成的 DMA 模板读写：
  - `BSP_SD_ReadBlocks_DMA(...)`
  - `BSP_SD_WriteBlocks_DMA(...)`

## 为什么不能直接用 CubeMX 默认模板

CubeMX 生成的 `sd_diskio.c` 默认通常是：

- DMA 模板版本
- `SD_TIMEOUT = 30 * 1000`

这会在你的项目里带来两个问题：

1. 挂载阶段 `f_mount()` 容易卡住
2. 一旦出错，主循环会长时间看起来像“死机”

## 建议做法

每次 CubeMX 重新生成后，直接把整个 `FATFS/Target/sd_diskio.c` 用当前项目稳定版本覆盖回去。

也就是说：

- 不建议只改一两行
- 建议把整个文件当作“项目定制文件”维护

如果以后再次生成，优先恢复整个文件。

---

## 2.4 `cmake/stm32cubemx/CMakeLists.txt`

必须保证自定义源文件已加入 `MX_Application_Src`，否则源码修改不会参与编译。

必须包含至少这些文件：

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/four_g_mqtt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/audio_uploader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/base64_codec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/audio_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/recorder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/wav_format.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/lcd_display.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/audio_detect.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/st7735.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/fonts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/gps.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/tts_player.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/tts_stop_record.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/tts_start_record.c
```

如果缺少这些项，表现通常是：

- 改了源码但现象没变化
- 实际烧录的不是最新逻辑

---

# 3. 推荐放进 USER CODE 区的代码

以下内容适合放到 `USER CODE` 区域中，避免 CubeMX 重新生成时丢失。

---

## 3.1 `Core/Src/main.c`

### 放到 `USER CODE BEGIN Includes`

```c
#include "stdio.h"
#include "st7735.h"
#include "fonts.h"
#include "led.h"
#include "audio_detect.h"
#include "lcd_display.h"
#include "gps.h"
#include "recorder.h"
#include "key.h"
```

### 放到 `USER CODE BEGIN PFP`

```c
// 保留旧声音检测功能相关缓冲区定义（当前不启用）
// #define ADC_BUFFER_SIZE  128
// uint16_t adc_buffer1[ADC_BUFFER_SIZE];
// uint16_t adc_buffer2[ADC_BUFFER_SIZE];

/* 录音模块全局上下文：负责采样、缓存和写卡状态 */
static Recorder_t gRecorder;
```

### 放到 `USER CODE BEGIN 2`

```c
ST7735_Init();
LED_Init();

/* 不在上电阶段初始化录音存储链路，避免 SD/FATFS 上电抖动影响主循环 */

// 初始化 LCD 显示
LCD_DisplayInit();

// 初始化 GPS 模块
GPS_Init();
clrStruct();
```

### 放到 `USER CODE BEGIN WHILE`

以下代码块可整体放回主循环：

```c
// 显示状态标志
static bool soundDisplayState = false;      // 声音检测显示状态（旧功能保留）
static bool gpsDisplayState = false;        // GPS 显示状态
static bool stopMsgActive = false;          // 停止录音提示是否处于显示中
static uint32_t stopMsgStartTick = 0U;      // 停止录音提示起始时间
static bool errorMsgActive = false;         // SD 错误提示显示中
static uint32_t errorMsgStartTick = 0U;     // SD 错误提示起始时间
static uint8_t key0PrevPressed = 0U;        // KEY0 上一次稳定按下状态
static uint8_t key1PrevPressed = 0U;        // KEY1 上一次稳定按下状态
uint8_t key0Pressed = 0U;
uint8_t key1Pressed = 0U;
RecorderResult_t rec_ret = RECORDER_OK;

while (1)
{
  LED_Process();

  /* 录音过程中持续搬运双缓冲数据并写入 SD */
  if (Recorder_GetState(&gRecorder) == RECORDER_STATE_RECORDING) {
    rec_ret = Recorder_Process(&gRecorder);
    if (rec_ret != RECORDER_OK) {
      LCD_DisplayStorageErrorDetail(Recorder_GetLastErrorStage(&gRecorder), Recorder_GetLastFresult(&gRecorder));
      errorMsgActive = true;
      errorMsgStartTick = HAL_GetTick();
    }
  }

  /*
  // 旧功能保留（按你要求不删除）：声音检测并切屏显示
  SoundDetector_Process(&gSoundDetector);
  if (SoundDetector_IsDetected(&gSoundDetector))
  {
    if (!soundDisplayState)
    {
      LCD_DisplaySoundDetected();
      soundDisplayState = true;
    }
  }
  else
  {
    if (soundDisplayState)
    {
      soundDisplayState = false;
      LCD_DisplayWaiting();
    }
  }
  */
  (void)soundDisplayState;

  /* 独立读取两个按键，避免共享状态机导致 KEY1 被漏检 */
  key0Pressed = (KEY0 == KEY0_ON) ? 1U : 0U;
  key1Pressed = (KEY1 == KEY1_ON) ? 1U : 0U;

  if ((key0Pressed != 0U) && (key0PrevPressed == 0U)) {
    /* KEY0(PC13): 开始录音 */
    if ((Recorder_GetState(&gRecorder) == RECORDER_STATE_IDLE) ||
        (Recorder_GetState(&gRecorder) == RECORDER_STATE_ERROR)) {
      rec_ret = Recorder_Init(&gRecorder);
      if (rec_ret != RECORDER_OK) {
        LCD_DisplayStorageErrorDetail(Recorder_GetLastErrorStage(&gRecorder), Recorder_GetLastFresult(&gRecorder));
        errorMsgActive = true;
        errorMsgStartTick = HAL_GetTick();
        key0PrevPressed = key0Pressed;
        key1PrevPressed = key1Pressed;
        continue;
      }
    }
    if (Recorder_GetState(&gRecorder) == RECORDER_STATE_READY) {
      rec_ret = Recorder_Start(&gRecorder, "REC0001.WAV", 0U);
      if (rec_ret == RECORDER_OK) {
        LCD_DisplayRecordingStarted();
        stopMsgActive = false;
        errorMsgActive = false;
      } else {
        LCD_DisplayStorageErrorDetail(Recorder_GetLastErrorStage(&gRecorder), Recorder_GetLastFresult(&gRecorder));
        errorMsgActive = true;
        errorMsgStartTick = HAL_GetTick();
      }
    }
  }

  if ((key1Pressed != 0U) && (key1PrevPressed == 0U)) {
    /* KEY1(PA0): 停止录音并回填 WAV 头 */
    if (Recorder_GetState(&gRecorder) == RECORDER_STATE_RECORDING) {
      rec_ret = Recorder_Stop(&gRecorder);
      if (rec_ret == RECORDER_OK) {
        LCD_DisplayRecordingStopped();
        stopMsgActive = true;
        stopMsgStartTick = HAL_GetTick();
        errorMsgActive = false;
      } else {
        LCD_DisplayStorageErrorDetail(Recorder_GetLastErrorStage(&gRecorder), Recorder_GetLastFresult(&gRecorder));
        errorMsgActive = true;
        errorMsgStartTick = HAL_GetTick();
      }
    }
  }

  key0PrevPressed = key0Pressed;
  key1PrevPressed = key1Pressed;

  if (stopMsgActive && ((HAL_GetTick() - stopMsgStartTick) >= 2000U)) {
    LCD_DisplayInit();
    stopMsgActive = false;
  }

  if (errorMsgActive && ((HAL_GetTick() - errorMsgStartTick) >= 2000U)) {
    LCD_DisplayInit();
    errorMsgActive = false;
  }

  // 处理 GPS 数据
  parseGpsBuffer();
  printGpsBuffer();

  // 处理 GPS 显示
  if ((Save_Data.isUsefull || !gpsDisplayState) && !stopMsgActive && !errorMsgActive)
  {
    LCD_DisplayGPS();
    gpsDisplayState = true;
  }
}
```

### 放到 `USER CODE BEGIN 4`

```c
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
  if (hadc->Instance != ADC1) {
    return;
  }

  /* DMA 半传输中断：标记并搬运前半缓冲 */
  Recorder_OnAdcHalfComplete(&gRecorder);

  // 旧功能保留（不启用）：
  // SoundDetector_OnAdcHalfComplete(&gSoundDetector);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  if (hadc->Instance != ADC1) {
    return;
  }

  /* DMA 全传输中断：标记并搬运后半缓冲 */
  Recorder_OnAdcFullComplete(&gRecorder);

  // 旧功能保留（不启用）：
  // SoundDetector_OnAdcFullComplete(&gSoundDetector);
}
```

---

# 4. 哪些内容不建议依赖 USER CODE，而应直接恢复文件

以下文件不建议只改局部 USER 区，因为 CubeMX 生成结构变化时容易出问题：

- `FATFS/Target/sd_diskio.c`
- `cmake/stm32cubemx/CMakeLists.txt`

建议做法：

## `sd_diskio.c`
每次重新生成后，直接用本项目稳定版本整体覆盖。

## `CMakeLists.txt`
每次重新生成后，检查自定义源文件是否还在 `MX_Application_Src` 列表里。

---

# 5. 重新生成后推荐检查顺序

1. 检查 `gpio.c`
   - `PC13/PA0` 是否还是 `GPIO_PULLUP`
2. 检查 `sdmmc.c`
   - `ClockDiv` 是否仍为 `118`
   - `CMD/D0` 是否仍为 `GPIO_PULLUP`
3. 检查 `sd_diskio.c`
   - 是否还是轮询版
   - `SD_TIMEOUT` 是否仍为 `1000`
4. 检查 `main.c`
   - `USER CODE` 中录音逻辑是否仍在
5. 检查 `CMakeLists.txt`
   - 自定义源文件是否仍加入编译
6. 进行一次 `Clean + Rebuild`

---

# 6. 建议

为了减少后续维护成本，建议将以下文件作为“项目定制文件”重点备份：

- `FATFS/Target/sd_diskio.c`
- `cmake/stm32cubemx/CMakeLists.txt`
- `Core/Src/main.c` 中 `USER CODE` 相关逻辑
- `Core/Src/sdmmc.c`
- `Core/Src/gpio.c`

这样每次 CubeMX 重新生成后，只需对照本文档恢复即可。
