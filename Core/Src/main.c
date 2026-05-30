/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "fatfs.h"
#include "sdmmc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "st7735.h"
#include "fonts.h"
#include "led.h"
#include "audio_detect.h"
#include "lcd_display.h"
#include "gps.h"
#include "recorder.h"
#include "key.h"
#include "tts_player.h"
#include "four_g_mqtt.h"
#include "audio_uploader.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  GPS_UPLOAD_STATE_IDLE = 0,
  GPS_UPLOAD_STATE_CONNECT_WAIT,
  GPS_UPLOAD_STATE_TIME_WAIT,
  GPS_UPLOAD_STATE_PUBLISH_WAIT,
  GPS_UPLOAD_STATE_DISCONNECT_WAIT
} GpsUploadState_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static GpsUploadState_t gGpsUploadState = GPS_UPLOAD_STATE_IDLE;
static float gPendingGpsLatitude = 0.0f;
static float gPendingGpsLongitude = 0.0f;
static char gPendingGpsTimestamp[24];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */
// 保留旧声音检测功能相关缓冲区定义（当前不启用）
// #define ADC_BUFFER_SIZE  128
// uint16_t adc_buffer1[ADC_BUFFER_SIZE];
// uint16_t adc_buffer2[ADC_BUFFER_SIZE];

/* 录音模块全局上下文：负责采样、缓存和写卡状态 */
static Recorder_t gRecorder;
#define RECORD_SLICE_DURATION_MS  10000U
static uint32_t gRecordingSessionId = 0U;
static uint32_t gCurrentSessionId = 0U;
static uint16_t gCurrentSliceIndex = 0U;
static uint32_t gSliceStartTick = 0U;
static char gCurrentWavName[AUDIO_UPLOADER_FILENAME_MAX];

static void BuildWavFilename(char *buffer, uint16_t buffer_size, uint32_t session_id, uint16_t slice_index)
{
  if ((buffer == NULL) || (buffer_size == 0U)) {
    return;
  }

  (void)snprintf(buffer, buffer_size, "%lu_%04u.wav", (unsigned long)session_id, (unsigned int)slice_index);
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void MX_TIM2_Init(void);
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI3_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_SDMMC1_SD_Init();
  MX_FATFS_Init();
  MX_DAC1_Init();
  MX_TIM6_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  FourG_MQTT_Init(&huart1);
  AudioUploader_Init();

  ST7735_Init();
  LED_Init();
  TTS_Player_Init();

  /* 不在上电阶段初始化录音存储链路，避免 SD/FATFS 上电抖动影响主循环 */

  // 初始化 LCD 显示
  LCD_DisplayInit();

  // 初始化 GPS 模块
  GPS_Init();
  clrStruct();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  // 显示状态标志
  static bool soundDisplayState = false;      // 声音检测显示状态（旧功能保留）
  static bool gpsDisplayState = false;        // GPS 显示状态
  static uint32_t lastUploadDisplayTick = 0U; // 上一次上传状态刷新时间
  static uint32_t uploadEnableTick = 0U;      // 停止录音后延迟启动上传的时间点
  static bool stopMsgActive = false;          // 停止录音提示是否处于显示中
  static uint32_t stopMsgStartTick = 0U;      // 停止录音提示起始时间
  static bool errorMsgActive = false;         // SD 错误提示显示中
  static uint32_t errorMsgStartTick = 0U;     // SD 错误提示起始时间
  static bool pendingStartRecordingAfterTts = false;
  static bool waitingStartRecordingDelay = false;
  static uint32_t startRecordingDelayTick = 0U;
  static uint8_t key0PrevPressed = 0U;        // KEY0 上一次稳定按下状态
  static uint8_t key1PrevPressed = 0U;        // KEY1 上一次稳定按下状态
  static uint8_t key2PrevPressed = 0U;        // KEY2 上一次稳定按下状态
  static bool workCardDisplayActive = false;  // 电子工牌测试界面显示中
  static uint32_t workCardClockTick = 0U;     // 电子工牌软件时钟节拍
  static uint8_t workCardHour = 15U;
  static uint8_t workCardMinute = 47U;
  static uint8_t workCardSecond = 0U;
  static uint32_t lastGpsUploadTick = 0U;     // 上一次 4G 上传 GPS 的时间
  static bool gpsLocationUploaded = false;    // 是否已经上传过有效 GPS 位置
  uint8_t key0Pressed = 0U;
  uint8_t key1Pressed = 0U;
  uint8_t key2Pressed = 0U;
  RecorderResult_t rec_ret = RECORDER_OK;

  while (1)
  {
    // LED_Process();

    /* 开始录音提示音结束后，延迟 10ms 再真正启动录音上传，避免提示音进入录音并让外设状态稳定 */
    if (pendingStartRecordingAfterTts && !TTS_Player_IsBusy()) {
      pendingStartRecordingAfterTts = false;
      waitingStartRecordingDelay = true;
      startRecordingDelayTick = HAL_GetTick();
    }

    if (waitingStartRecordingDelay && ((HAL_GetTick() - startRecordingDelayTick) >= 10U)) {
      waitingStartRecordingDelay = false;

      if ((Recorder_GetState(&gRecorder) == RECORDER_STATE_IDLE) ||
          (Recorder_GetState(&gRecorder) == RECORDER_STATE_ERROR)) {
        rec_ret = Recorder_Init(&gRecorder);
        if (rec_ret != RECORDER_OK) {
          LCD_DisplayStorageErrorDetail(Recorder_GetLastErrorStage(&gRecorder), Recorder_GetLastFresult(&gRecorder));
          errorMsgActive = true;
          errorMsgStartTick = HAL_GetTick();
          continue;
        }
      }

      if (Recorder_GetState(&gRecorder) == RECORDER_STATE_READY) {
        AudioUploader_Abort();
        uploadEnableTick = 0U;
        gRecordingSessionId++;
        if (gRecordingSessionId == 0U) {
          gRecordingSessionId = 1U;
        }
        gCurrentSessionId = gRecordingSessionId;
        gCurrentSliceIndex = 1U;
        BuildWavFilename(gCurrentWavName, sizeof(gCurrentWavName), gCurrentSessionId, gCurrentSliceIndex);

        rec_ret = Recorder_Start(&gRecorder, gCurrentWavName, 0U);
        if (rec_ret == RECORDER_OK) {
          gSliceStartTick = HAL_GetTick();
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

    /* 录音过程中持续搬运双缓冲数据并写入 SD */
    if (Recorder_GetState(&gRecorder) == RECORDER_STATE_RECORDING) {
      rec_ret = Recorder_Process(&gRecorder);
      if (rec_ret != RECORDER_OK) {
        LCD_DisplayStorageErrorDetail(Recorder_GetLastErrorStage(&gRecorder), Recorder_GetLastFresult(&gRecorder));
        errorMsgActive = true;
        errorMsgStartTick = HAL_GetTick();
      } else if ((HAL_GetTick() - gSliceStartTick) >= RECORD_SLICE_DURATION_MS) {
        char completedWavName[AUDIO_UPLOADER_FILENAME_MAX];
        char nextWavName[AUDIO_UPLOADER_FILENAME_MAX];

        (void)snprintf(completedWavName, sizeof(completedWavName), "%s", gCurrentWavName);
        gCurrentSliceIndex++;
        BuildWavFilename(nextWavName, sizeof(nextWavName), gCurrentSessionId, gCurrentSliceIndex);

        rec_ret = Recorder_SwitchFile(&gRecorder, nextWavName);
        if (rec_ret == RECORDER_OK) {
          (void)AudioUploader_EnqueueFile(completedWavName, gCurrentSessionId, false);
          (void)snprintf(gCurrentWavName, sizeof(gCurrentWavName), "%s", nextWavName);
          gSliceStartTick = HAL_GetTick();
        } else {
          LCD_DisplayStorageErrorDetail(Recorder_GetLastErrorStage(&gRecorder), Recorder_GetLastFresult(&gRecorder));
          errorMsgActive = true;
          errorMsgStartTick = HAL_GetTick();
        }
      }
    }

    /*
    // 旧功能保留：声音检测并切屏显示
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

    /* 独立读取三个按键，避免共享状态机导致按键被漏检 */
    key0Pressed = (KEY0 == KEY0_ON) ? 1U : 0U;
    key1Pressed = (KEY1 == KEY1_ON) ? 1U : 0U;
    key2Pressed = (KEY2 == KEY2_ON) ? 1U : 0U;

    if ((key0Pressed != 0U) && (key0PrevPressed == 0U)) {
      /* KEY0(PA7): 开始录音 */
      if ((Recorder_GetState(&gRecorder) != RECORDER_STATE_RECORDING) &&
          !pendingStartRecordingAfterTts &&
          !waitingStartRecordingDelay &&
          !TTS_Player_IsBusy()) {
        if (TTS_Player_PlayStartRecord()) {
          workCardDisplayActive = false;
          pendingStartRecordingAfterTts = true;
          stopMsgActive = false;
          errorMsgActive = false;
        }
      }
    }

    if ((key1Pressed != 0U) && (key1PrevPressed == 0U)) {
      /* KEY1(PA6): 停止录音并回填 WAV 头 */
      if (Recorder_GetState(&gRecorder) == RECORDER_STATE_RECORDING) {
        rec_ret = Recorder_Stop(&gRecorder);
        if (rec_ret == RECORDER_OK) {
          if (gCurrentSessionId != 0U) {
            (void)AudioUploader_EnqueueFile(gCurrentWavName, gCurrentSessionId, true);
            AudioUploader_MarkLastQueuedFileFinal(gCurrentSessionId);
          }
          workCardDisplayActive = false;
          LCD_DisplayRecordingStopped();
          stopMsgActive = true;
          stopMsgStartTick = HAL_GetTick();
          uploadEnableTick = HAL_GetTick() + 5000U;
          errorMsgActive = false;
          gCurrentSessionId = 0U;
          gCurrentSliceIndex = 0U;
          (void)TTS_Player_PlayStopRecord();
        } else {
          LCD_DisplayStorageErrorDetail(Recorder_GetLastErrorStage(&gRecorder), Recorder_GetLastFresult(&gRecorder));
          errorMsgActive = true;
          errorMsgStartTick = HAL_GetTick();
        }
      }
    }

    if ((key2Pressed != 0U) && (key2PrevPressed == 0U)) {
      /* KEY2(PA5): 进入电子工牌测试界面 */
      workCardDisplayActive = true;
      workCardClockTick = HAL_GetTick();
      workCardHour = 15U;
      workCardMinute = 47U;
      workCardSecond = 0U;
      stopMsgActive = false;
      errorMsgActive = false;
      gpsDisplayState = false;
      LCD_DisplayWorkCardInit();
    }

    if (workCardDisplayActive && ((HAL_GetTick() - workCardClockTick) >= 1000U)) {
      workCardClockTick += 1000U;
      workCardSecond++;
      if (workCardSecond >= 60U) {
        workCardSecond = 0U;
        workCardMinute++;
        if (workCardMinute >= 60U) {
          workCardMinute = 0U;
          workCardHour = (workCardHour + 1U) % 24U;
        }
        LCD_DisplayClockValue(workCardHour, workCardMinute);
      }
    }

    key0PrevPressed = key0Pressed;
    key1PrevPressed = key1Pressed;
    key2PrevPressed = key2Pressed;

    if (stopMsgActive && ((HAL_GetTick() - stopMsgStartTick) >= 2000U)) {
      LCD_DisplayInit();
      stopMsgActive = false;
    }

    if (errorMsgActive && ((HAL_GetTick() - errorMsgStartTick) >= 2000U)) {
      LCD_DisplayInit();
      errorMsgActive = false;
    }

    // 处理 GPS 数据
    {
      uint8_t gpsParsed = 0U;

      parseGpsBuffer();
      if (Save_Data.isParseData)
      {
        gpsParsed = 1U;
      }

      switch (gGpsUploadState)
      {
        case GPS_UPLOAD_STATE_CONNECT_WAIT:
        {
          FourGMqttAsyncStatus_t status = FourG_MQTT_AsyncProcess();
          if (status == FOUR_G_MQTT_ASYNC_DONE)
          {
            if (FourG_MQTT_AsyncGetUnixTimeStart() == FOUR_G_MQTT_OK)
            {
              gGpsUploadState = GPS_UPLOAD_STATE_TIME_WAIT;
            }
            else
            {
              (void)snprintf(gPendingGpsTimestamp, sizeof(gPendingGpsTimestamp), "1970-01-01T00:00:00Z");
              if (FourG_MQTT_AsyncPublishLocationStart(gPendingGpsLatitude, gPendingGpsLongitude, gPendingGpsTimestamp) == FOUR_G_MQTT_OK)
              {
                gGpsUploadState = GPS_UPLOAD_STATE_PUBLISH_WAIT;
              }
              else if (FourG_MQTT_AsyncDisconnectStart() == FOUR_G_MQTT_OK)
              {
                gGpsUploadState = GPS_UPLOAD_STATE_DISCONNECT_WAIT;
              }
              else
              {
                lastGpsUploadTick = HAL_GetTick();
                gGpsUploadState = GPS_UPLOAD_STATE_IDLE;
              }
            }
          }
          else if (status == FOUR_G_MQTT_ASYNC_ERROR)
          {
            if (FourG_MQTT_AsyncDisconnectStart() == FOUR_G_MQTT_OK)
            {
              gGpsUploadState = GPS_UPLOAD_STATE_DISCONNECT_WAIT;
            }
            else
            {
              lastGpsUploadTick = HAL_GetTick();
              gGpsUploadState = GPS_UPLOAD_STATE_IDLE;
            }
          }
          break;
        }

        case GPS_UPLOAD_STATE_TIME_WAIT:
        {
          FourGMqttAsyncStatus_t status = FourG_MQTT_AsyncProcess();
          if (status == FOUR_G_MQTT_ASYNC_DONE)
          {
            if (!FourG_MQTT_AsyncGetTimestamp(gPendingGpsTimestamp, sizeof(gPendingGpsTimestamp)))
            {
              (void)snprintf(gPendingGpsTimestamp, sizeof(gPendingGpsTimestamp), "1970-01-01T00:00:00Z");
            }

            if (FourG_MQTT_AsyncPublishLocationStart(gPendingGpsLatitude, gPendingGpsLongitude, gPendingGpsTimestamp) == FOUR_G_MQTT_OK)
            {
              gGpsUploadState = GPS_UPLOAD_STATE_PUBLISH_WAIT;
            }
            else if (FourG_MQTT_AsyncDisconnectStart() == FOUR_G_MQTT_OK)
            {
              gGpsUploadState = GPS_UPLOAD_STATE_DISCONNECT_WAIT;
            }
            else
            {
              lastGpsUploadTick = HAL_GetTick();
              gGpsUploadState = GPS_UPLOAD_STATE_IDLE;
            }
          }
          else if (status == FOUR_G_MQTT_ASYNC_ERROR)
          {
            (void)snprintf(gPendingGpsTimestamp, sizeof(gPendingGpsTimestamp), "1970-01-01T00:00:00Z");
            if (FourG_MQTT_AsyncPublishLocationStart(gPendingGpsLatitude, gPendingGpsLongitude, gPendingGpsTimestamp) == FOUR_G_MQTT_OK)
            {
              gGpsUploadState = GPS_UPLOAD_STATE_PUBLISH_WAIT;
            }
            else if (FourG_MQTT_AsyncDisconnectStart() == FOUR_G_MQTT_OK)
            {
              gGpsUploadState = GPS_UPLOAD_STATE_DISCONNECT_WAIT;
            }
            else
            {
              lastGpsUploadTick = HAL_GetTick();
              gGpsUploadState = GPS_UPLOAD_STATE_IDLE;
            }
          }
          break;
        }

        case GPS_UPLOAD_STATE_PUBLISH_WAIT:
        {
          FourGMqttAsyncStatus_t status = FourG_MQTT_AsyncProcess();
          if (status == FOUR_G_MQTT_ASYNC_DONE)
          {
            lastGpsUploadTick = HAL_GetTick();
            gpsLocationUploaded = true;
            gGpsUploadState = GPS_UPLOAD_STATE_IDLE;
          }
          else if (status == FOUR_G_MQTT_ASYNC_ERROR)
          {
            if (FourG_MQTT_AsyncDisconnectStart() == FOUR_G_MQTT_OK)
            {
              gGpsUploadState = GPS_UPLOAD_STATE_DISCONNECT_WAIT;
            }
            else
            {
              lastGpsUploadTick = HAL_GetTick();
              gGpsUploadState = GPS_UPLOAD_STATE_IDLE;
            }
          }
          break;
        }

        case GPS_UPLOAD_STATE_DISCONNECT_WAIT:
        {
          FourGMqttAsyncStatus_t status = FourG_MQTT_AsyncProcess();
          if (status != FOUR_G_MQTT_ASYNC_BUSY)
          {
            lastGpsUploadTick = HAL_GetTick();
            gGpsUploadState = GPS_UPLOAD_STATE_IDLE;
          }
          break;
        }

        case GPS_UPLOAD_STATE_IDLE:
        default:
          break;
      }

      if (false &&
          (gGpsUploadState == GPS_UPLOAD_STATE_IDLE) &&
          (gpsParsed != 0U) && (Save_Data.isUsefull) &&
          (Recorder_GetState(&gRecorder) != RECORDER_STATE_RECORDING) &&
          !stopMsgActive && !errorMsgActive &&
          (!gpsLocationUploaded || ((HAL_GetTick() - lastGpsUploadTick) >= 30000U)))
      {
        float latitude = ConvertLatitude(Save_Data.latitude, Save_Data.N_S[0]);
        float longitude = ConvertLongitude(Save_Data.longitude, Save_Data.E_W[0]);

        if ((latitude != 0.0f) && (longitude != 0.0f) && !AudioUploader_IsBusy())
        {
          gPendingGpsLatitude = latitude;
          gPendingGpsLongitude = longitude;
          if (FourG_MQTT_AsyncEnsureConnectedStart() == FOUR_G_MQTT_OK)
          {
            gGpsUploadState = GPS_UPLOAD_STATE_CONNECT_WAIT;
          }
          else
          {
            lastGpsUploadTick = HAL_GetTick();
          }
        }
      }

      if (!workCardDisplayActive && !stopMsgActive && !errorMsgActive &&
          (Recorder_GetState(&gRecorder) != RECORDER_STATE_RECORDING))
      {
        if ((gpsParsed != 0U) || !gpsDisplayState)
        {
          LCD_DisplayGPS();
          gpsDisplayState = true;
        }
      }

      if (gpsParsed != 0U)
      {
        Save_Data.isParseData = false;
      }

      /* GPS printf debug is disabled because __io_putchar is not retargeted in this project. */
    }

    // GPS 调试信息：显示接收计数（始终显示）
    // {
    //   static uint32_t lastRxCount = 0;
    //   static uint32_t lastDisplayTick = 0;
    //   uint32_t currentRxCount = GPS_GetRxCount();

    //   if (HAL_GetTick() - lastDisplayTick > 1000) {
    //     lastDisplayTick = HAL_GetTick();

    //     ST7735_FillRect(0, 64, ST7735_WIDTH, 16, ST7735_BLACK);

    //     if (currentRxCount != lastRxCount) {
    //       LCD_DisplayDebug(0, "GPS Rx: %lu bytes", currentRxCount);
    //       lastRxCount = currentRxCount;
    //     } else if (currentRxCount > 0) {
    //       LCD_DisplayDebug(0, "GPS Rx: %lu live", currentRxCount);
    //     } else {
    //       LCD_DisplayDebug(0, "GPS Rx: waiting");
    //     }
    //   }
    // }

    if (!workCardDisplayActive &&
        (Recorder_GetState(&gRecorder) != RECORDER_STATE_RECORDING) &&
        !stopMsgActive && !errorMsgActive) {
      if ((HAL_GetTick() - lastUploadDisplayTick) >= 1000U) {
        lastUploadDisplayTick = HAL_GetTick();
        LCD_DisplayUploadStatus();
      }

      if ((uploadEnableTick == 0U) || ((int32_t)(HAL_GetTick() - uploadEnableTick) >= 0)) {
        AudioUploader_Process();
      }
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SDMMC1|RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
  PeriphClkInit.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_PLLSAI1;
  PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_HSE;
  PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
  PeriphClkInit.PLLSAI1.PLLSAI1N = 8;
  PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
  PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV4;
  PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_48M2CLK|RCC_PLLSAI1_ADC1CLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
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

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
