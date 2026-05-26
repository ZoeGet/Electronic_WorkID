#include "tts_player.h"

#include "dac.h"
#include "tim.h"
#include "tts_audio_data.h"

#define TTS_SAMPLE_RATE_HZ       16000U
#define TTS_DAC_MID              2048U
#define TTS_SOFTWARE_GAIN        8
#define TTS_TEST_TONE_SAMPLES    16000U
#define TTS_TEST_TONE_HALF_CYCLE 8U

static volatile bool g_ttsBusy = false;
static volatile bool g_ttsTestToneMode = false;
static volatile const uint16_t *g_ttsData = NULL;
static volatile uint32_t g_ttsLength = 0U;
static volatile uint32_t g_ttsIndex = 0U;

static inline void TTS_Player_WriteDac(uint16_t sample)
{
  hdac1.Instance->DHR12R1 = ((uint32_t)sample & 0x0FFFU);
}

static uint16_t TTS_Player_ApplyGain(uint16_t sample)
{
  int32_t centered = (int32_t)sample - (int32_t)TTS_DAC_MID;
  int32_t amplified = (int32_t)TTS_DAC_MID + (centered * TTS_SOFTWARE_GAIN);

  if (amplified < 256) {
    return 256U;
  }

  if (amplified > 3840) {
    return 3840U;
  }

  return (uint16_t)amplified;
}

static uint32_t TTS_Player_GetTim6ClockHz(void)
{
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

  if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_HCLK_DIV1) {
    return pclk1 * 2U;
  }

  return pclk1;
}

static void TTS_Player_ConfigTim6(uint32_t sample_rate_hz)
{
  uint32_t period = 0U;
  uint32_t tim_clock_hz = TTS_Player_GetTim6ClockHz();

  if (sample_rate_hz == 0U) {
    sample_rate_hz = TTS_SAMPLE_RATE_HZ;
  }

  period = (tim_clock_hz / sample_rate_hz);
  if (period > 0U) {
    period -= 1U;
  }

  (void)HAL_TIM_Base_Stop_IT(&htim6);
  __HAL_TIM_SET_PRESCALER(&htim6, 0U);
  __HAL_TIM_SET_AUTORELOAD(&htim6, period);
  __HAL_TIM_SET_COUNTER(&htim6, 0U);
  __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
}

static void TTS_Player_ConfigSoftwareOutput(void)
{
  DAC_ChannelConfTypeDef sConfig = {0};

  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;

  (void)HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1);
}

static bool TTS_Player_StartInterruptPlayback(const uint16_t *data, uint32_t length, bool test_tone_mode)
{
  if (((data == NULL) && !test_tone_mode) || (length == 0U) || g_ttsBusy) {
    return false;
  }

  (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
  (void)HAL_TIM_Base_Stop_IT(&htim6);

  TTS_Player_ConfigSoftwareOutput();
  TTS_Player_ConfigTim6(TTS_SAMPLE_RATE_HZ);

  g_ttsData = data;
  g_ttsLength = length;
  g_ttsIndex = 0U;
  g_ttsTestToneMode = test_tone_mode;
  g_ttsBusy = true;

  (void)HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
  TTS_Player_WriteDac(TTS_DAC_MID);

  if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
    g_ttsBusy = false;
    g_ttsTestToneMode = false;
    g_ttsData = NULL;
    g_ttsLength = 0U;
    g_ttsIndex = 0U;
    return false;
  }

  return true;
}

static bool TTS_Player_Play(const uint16_t *data, uint32_t length)
{
  return TTS_Player_StartInterruptPlayback(data, length, false);
}

void TTS_Player_Init(void)
{
  g_ttsBusy = false;
  g_ttsTestToneMode = false;
  g_ttsData = NULL;
  g_ttsLength = 0U;
  g_ttsIndex = 0U;

  (void)HAL_TIM_Base_Stop_IT(&htim6);
  (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
  TTS_Player_ConfigSoftwareOutput();
  TTS_Player_ConfigTim6(TTS_SAMPLE_RATE_HZ);
  (void)HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
  TTS_Player_WriteDac(TTS_DAC_MID);
}

bool TTS_Player_IsBusy(void)
{
  return g_ttsBusy;
}

bool TTS_Player_PlayStartRecord(void)
{
  return TTS_Player_Play(tts_start_record, tts_start_record_len);
}

bool TTS_Player_PlayStopRecord(void)
{
  return TTS_Player_Play(tts_stop_record, tts_stop_record_len);
}

bool TTS_Player_PlayTestTone(void)
{
  return TTS_Player_StartInterruptPlayback(NULL, TTS_TEST_TONE_SAMPLES, true);
}

void TTS_Player_PlayBlockingTestTone(void)
{
}

void TTS_Player_Stop(void)
{
  (void)HAL_TIM_Base_Stop_IT(&htim6);
  (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);

  g_ttsBusy = false;
  g_ttsTestToneMode = false;
  g_ttsData = NULL;
  g_ttsLength = 0U;
  g_ttsIndex = 0U;

  TTS_Player_ConfigSoftwareOutput();
  (void)HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
  TTS_Player_WriteDac(TTS_DAC_MID);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  uint16_t sample = TTS_DAC_MID;

  if ((htim == NULL) || (htim->Instance != TIM6)) {
    return;
  }

  if (!g_ttsBusy || (g_ttsIndex >= g_ttsLength)) {
    TTS_Player_Stop();
    return;
  }

  if (g_ttsTestToneMode) {
    if (((g_ttsIndex / TTS_TEST_TONE_HALF_CYCLE) % 2U) == 0U) {
      sample = 4095U;
    } else {
      sample = 0U;
    }
  } else if (g_ttsData != NULL) {
    sample = TTS_Player_ApplyGain(g_ttsData[g_ttsIndex]);
  }

  TTS_Player_WriteDac(sample);
  g_ttsIndex++;
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
  if ((hdac == NULL) || (hdac->Instance != DAC1)) {
    return;
  }

  TTS_Player_Stop();
}
