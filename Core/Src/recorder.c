#include "recorder.h"

#include "adc.h"
#include "fatfs.h"
#include "tim.h"
#include "wav_format.h"
#include <string.h>

/* 启用 SD/FATFS 写卡链路，录音保存为 WAV 文件 */
#define RECORDER_ENABLE_SD_STORAGE  1

/* 定期同步到介质，降低掉电时数据丢失窗口 */
#define REC_SYNC_CHUNK_BYTES  (64U * 1024U)

#if RECORDER_ENABLE_SD_STORAGE
static RecorderResult_t Recorder_WriteWavHeader(Recorder_t *recorder, uint32_t pcm_bytes)
{
  uint32_t wav_header_words[WAV_HEADER_SIZE / 4U];
  uint8_t *wav_header = (uint8_t *)wav_header_words;
  UINT written = 0;

  recorder->last_error_stage = RECORDER_STAGE_REWRITE_HEADER;
  WAV_BuildHeader(wav_header, REC_SAMPLE_RATE, REC_BITS_PER_SAMPLE, REC_CHANNELS, pcm_bytes);

  recorder->last_fresult = f_lseek(&recorder->file, 0U);
  if (recorder->last_fresult != FR_OK) {
    return RECORDER_ERR_IO;
  }

  recorder->last_fresult = f_write(&recorder->file, wav_header, WAV_HEADER_SIZE, &written);
  if ((recorder->last_fresult != FR_OK) || (written != WAV_HEADER_SIZE)) {
    return RECORDER_ERR_IO;
  }

  return RECORDER_OK;
}

static RecorderResult_t Recorder_DrainReadyBlocks(Recorder_t *recorder)
{
  uint8_t *block = NULL;
  uint32_t size = 0U;
  UINT written = 0;

  while (AudioBuffer_GetReadyBlock(&recorder->audio, &block, &size)) {
    recorder->last_error_stage = RECORDER_STAGE_WRITE_DATA;
    recorder->last_fresult = f_write(&recorder->file, block, size, &written);
    if ((recorder->last_fresult != FR_OK) || (written != size)) {
      recorder->state = RECORDER_STATE_ERROR;
      return RECORDER_ERR_IO;
    }

    recorder->pcm_bytes_written += size;
    recorder->sync_acc_bytes += size;
  }

  return RECORDER_OK;
}
#endif

RecorderResult_t Recorder_Init(Recorder_t *recorder)
{
  if (recorder == NULL) {
    return RECORDER_ERR_STATE;
  }

  memset(recorder, 0, sizeof(*recorder));
  AudioBuffer_Init(&recorder->audio);

#if RECORDER_ENABLE_SD_STORAGE
  recorder->last_error_stage = RECORDER_STAGE_MOUNT;

  recorder->last_fresult = f_mount(&SDFatFS, (TCHAR const *)SDPath, 1U);
  if (recorder->last_fresult != FR_OK) {
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_FATFS;
  }
#else
  recorder->last_fresult = FR_OK;
#endif

  recorder->last_error_stage = RECORDER_STAGE_NONE;
  recorder->state = RECORDER_STATE_READY;
  return RECORDER_OK;
}

RecorderResult_t Recorder_Start(Recorder_t *recorder, const char *filename, uint32_t expected_pcm_bytes)
{
#if RECORDER_ENABLE_SD_STORAGE
  uint32_t wav_header_words[WAV_HEADER_SIZE / 4U];
  uint8_t *wav_header = (uint8_t *)wav_header_words;
  UINT written = 0;
#else
  (void)filename;
  (void)expected_pcm_bytes;
#endif

  if (recorder == NULL) {
    return RECORDER_ERR_STATE;
  }

#if RECORDER_ENABLE_SD_STORAGE
  if (filename == NULL) {
    return RECORDER_ERR_STATE;
  }
#endif

  if (recorder->state != RECORDER_STATE_READY) {
    return RECORDER_ERR_STATE;
  }

#if RECORDER_ENABLE_SD_STORAGE
  recorder->last_error_stage = RECORDER_STAGE_OPEN;
  recorder->last_fresult = f_open(&recorder->file, filename, FA_CREATE_ALWAYS | FA_WRITE);
  if (recorder->last_fresult != FR_OK) {
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_FATFS;
  }

  recorder->last_error_stage = RECORDER_STAGE_WRITE_HEADER;
  WAV_BuildHeader(wav_header, REC_SAMPLE_RATE, REC_BITS_PER_SAMPLE, REC_CHANNELS, 0U);
  recorder->last_fresult = f_write(&recorder->file, wav_header, WAV_HEADER_SIZE, &written);
  if ((recorder->last_fresult != FR_OK) || (written != WAV_HEADER_SIZE)) {
    f_close(&recorder->file);
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_IO;
  }

  if (expected_pcm_bytes > 0U) {
    recorder->last_error_stage = RECORDER_STAGE_PREALLOC;
    recorder->last_fresult = f_lseek(&recorder->file, WAV_HEADER_SIZE + expected_pcm_bytes);
    if (recorder->last_fresult == FR_OK) {
      recorder->last_fresult = f_lseek(&recorder->file, WAV_HEADER_SIZE);
    }
  }
#endif

  recorder->pcm_bytes_written = 0U;
  recorder->sync_acc_bytes = 0U;
  recorder->audio.ready_count = 0U;
  recorder->audio.overrun_count = 0U;
  recorder->audio.write_index = 0U;
  recorder->audio.read_index = 0U;

  /* 先开采样时基，再开 ADC DMA */
  recorder->last_error_stage = RECORDER_STAGE_START_TIM;
  if (HAL_TIM_Base_Start(&htim2) != HAL_OK) {
#if RECORDER_ENABLE_SD_STORAGE
    f_close(&recorder->file);
#endif
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_STATE;
  }

  recorder->last_error_stage = RECORDER_STAGE_START_ADC_DMA;
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)recorder->audio.dma_buffer, AUDIO_DMA_SAMPLES) != HAL_OK) {
    HAL_TIM_Base_Stop(&htim2);
#if RECORDER_ENABLE_SD_STORAGE
    f_close(&recorder->file);
#endif
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_STATE;
  }

  recorder->last_error_stage = RECORDER_STAGE_NONE;
  recorder->state = RECORDER_STATE_RECORDING;

  return RECORDER_OK;
}

RecorderResult_t Recorder_Process(Recorder_t *recorder)
{
  uint8_t *block = NULL;
  uint32_t size = 0U;
  UINT written = 0;
  uint32_t processed_blocks = 0U;

#if !RECORDER_ENABLE_SD_STORAGE
  (void)written;
#endif

  if ((recorder == NULL) || (recorder->state != RECORDER_STATE_RECORDING)) {
    return RECORDER_ERR_STATE;
  }

  /* 每次主循环最多处理两个 ready 数据块，保持稳定写卡节奏 */
  while ((processed_blocks < 2U) && AudioBuffer_GetReadyBlock(&recorder->audio, &block, &size)) {
#if RECORDER_ENABLE_SD_STORAGE
    recorder->last_error_stage = RECORDER_STAGE_WRITE_DATA;
    recorder->last_fresult = f_write(&recorder->file, block, size, &written);
    if ((recorder->last_fresult != FR_OK) || (written != size)) {
      recorder->state = RECORDER_STATE_ERROR;
      return RECORDER_ERR_IO;
    }
#else
    recorder->last_error_stage = RECORDER_STAGE_UPLOAD;
#endif

    recorder->pcm_bytes_written += size;
    recorder->sync_acc_bytes += size;
    processed_blocks++;

#if RECORDER_ENABLE_SD_STORAGE
    if (recorder->sync_acc_bytes >= REC_SYNC_CHUNK_BYTES) {
      /* 周期性同步，兼顾性能与可靠性 */
      recorder->sync_acc_bytes = 0U;
      recorder->last_error_stage = RECORDER_STAGE_SYNC;
      recorder->last_fresult = f_sync(&recorder->file);
      if (recorder->last_fresult != FR_OK) {
        recorder->state = RECORDER_STATE_ERROR;
        return RECORDER_ERR_IO;
      }
    }
#endif
  }

  recorder->last_error_stage = RECORDER_STAGE_NONE;
  return RECORDER_OK;
}

RecorderResult_t Recorder_SwitchFile(Recorder_t *recorder, const char *next_filename)
{
#if RECORDER_ENABLE_SD_STORAGE
  uint32_t wav_header_words[WAV_HEADER_SIZE / 4U];
  uint8_t *wav_header = (uint8_t *)wav_header_words;
  UINT written = 0;
#endif

  if ((recorder == NULL) || (next_filename == NULL) || (recorder->state != RECORDER_STATE_RECORDING)) {
    return RECORDER_ERR_STATE;
  }

  (void)HAL_ADC_Stop_DMA(&hadc1);
  (void)HAL_TIM_Base_Stop(&htim2);

#if RECORDER_ENABLE_SD_STORAGE
  RecorderResult_t ret = Recorder_DrainReadyBlocks(recorder);
  if (ret != RECORDER_OK) {
    recorder->last_error_stage = RECORDER_STAGE_CLOSE;
    (void)f_close(&recorder->file);
    return ret;
  }

  ret = Recorder_WriteWavHeader(recorder, recorder->pcm_bytes_written);
  if (ret != RECORDER_OK) {
    recorder->state = RECORDER_STATE_ERROR;
    recorder->last_error_stage = RECORDER_STAGE_CLOSE;
    (void)f_close(&recorder->file);
    return ret;
  }

  recorder->last_error_stage = RECORDER_STAGE_SYNC;
  recorder->last_fresult = f_sync(&recorder->file);
  if (recorder->last_fresult != FR_OK) {
    recorder->state = RECORDER_STATE_ERROR;
    recorder->last_error_stage = RECORDER_STAGE_CLOSE;
    (void)f_close(&recorder->file);
    return RECORDER_ERR_IO;
  }

  recorder->last_error_stage = RECORDER_STAGE_CLOSE;
  recorder->last_fresult = f_close(&recorder->file);
  if (recorder->last_fresult != FR_OK) {
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_IO;
  }

  recorder->last_error_stage = RECORDER_STAGE_OPEN;
  recorder->last_fresult = f_open(&recorder->file, next_filename, FA_CREATE_ALWAYS | FA_WRITE);
  if (recorder->last_fresult != FR_OK) {
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_FATFS;
  }

  recorder->last_error_stage = RECORDER_STAGE_WRITE_HEADER;
  WAV_BuildHeader(wav_header, REC_SAMPLE_RATE, REC_BITS_PER_SAMPLE, REC_CHANNELS, 0U);
  recorder->last_fresult = f_write(&recorder->file, wav_header, WAV_HEADER_SIZE, &written);
  if ((recorder->last_fresult != FR_OK) || (written != WAV_HEADER_SIZE)) {
    (void)f_close(&recorder->file);
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_IO;
  }
#else
  (void)next_filename;
#endif

  recorder->pcm_bytes_written = 0U;
  recorder->sync_acc_bytes = 0U;
  AudioBuffer_Init(&recorder->audio);

  recorder->last_error_stage = RECORDER_STAGE_START_TIM;
  if (HAL_TIM_Base_Start(&htim2) != HAL_OK) {
#if RECORDER_ENABLE_SD_STORAGE
    (void)f_close(&recorder->file);
#endif
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_STATE;
  }

  recorder->last_error_stage = RECORDER_STAGE_START_ADC_DMA;
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)recorder->audio.dma_buffer, AUDIO_DMA_SAMPLES) != HAL_OK) {
    HAL_TIM_Base_Stop(&htim2);
#if RECORDER_ENABLE_SD_STORAGE
    (void)f_close(&recorder->file);
#endif
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_STATE;
  }

  recorder->last_error_stage = RECORDER_STAGE_NONE;
  recorder->state = RECORDER_STATE_RECORDING;
  return RECORDER_OK;
}

RecorderResult_t Recorder_Stop(Recorder_t *recorder)
{
  RecorderResult_t ret = RECORDER_OK;

  if (recorder == NULL) {
    return RECORDER_ERR_STATE;
  }

  if (recorder->state != RECORDER_STATE_RECORDING) {
    return RECORDER_ERR_STATE;
  }

  /* 先停采样链路，再落盘收尾 */
  (void)HAL_ADC_Stop_DMA(&hadc1);
  (void)HAL_TIM_Base_Stop(&htim2);

#if RECORDER_ENABLE_SD_STORAGE
  ret = Recorder_DrainReadyBlocks(recorder);
  if (ret != RECORDER_OK) {
    recorder->last_error_stage = RECORDER_STAGE_CLOSE;
    (void)f_close(&recorder->file);
    return ret;
  }
#endif

#if RECORDER_ENABLE_SD_STORAGE
  ret = Recorder_WriteWavHeader(recorder, recorder->pcm_bytes_written);
  if (ret != RECORDER_OK) {
    recorder->state = RECORDER_STATE_ERROR;
    recorder->last_error_stage = RECORDER_STAGE_CLOSE;
    (void)f_close(&recorder->file);
    return ret;
  }

  recorder->last_error_stage = RECORDER_STAGE_SYNC;
  recorder->last_fresult = f_sync(&recorder->file);
  if (recorder->last_fresult != FR_OK) {
    recorder->state = RECORDER_STATE_ERROR;
    recorder->last_error_stage = RECORDER_STAGE_CLOSE;
    (void)f_close(&recorder->file);
    return RECORDER_ERR_IO;
  }

  recorder->last_error_stage = RECORDER_STAGE_CLOSE;
  recorder->last_fresult = f_close(&recorder->file);
  if (recorder->last_fresult != FR_OK) {
    recorder->state = RECORDER_STATE_ERROR;
    return RECORDER_ERR_IO;
  }
#else
  (void)ret;
#endif

  recorder->last_error_stage = RECORDER_STAGE_NONE;
  recorder->state = RECORDER_STATE_READY;
  return RECORDER_OK;
}

void Recorder_OnAdcHalfComplete(Recorder_t *recorder)
{
  if ((recorder != NULL) && (recorder->state == RECORDER_STATE_RECORDING)) {
    AudioBuffer_OnDmaHalfComplete(&recorder->audio);
  }
}

void Recorder_OnAdcFullComplete(Recorder_t *recorder)
{
  if ((recorder != NULL) && (recorder->state == RECORDER_STATE_RECORDING)) {
    AudioBuffer_OnDmaFullComplete(&recorder->audio);
  }
}

RecorderState_t Recorder_GetState(const Recorder_t *recorder)
{
  if (recorder == NULL) {
    return RECORDER_STATE_ERROR;
  }
  return recorder->state;
}

uint32_t Recorder_GetOverrunCount(const Recorder_t *recorder)
{
  if (recorder == NULL) {
    return 0U;
  }
  return recorder->audio.overrun_count;
}

uint16_t Recorder_GetLastRawMin(const Recorder_t *recorder)
{
  if (recorder == NULL) {
    return 0U;
  }
  return recorder->audio.last_raw_min;
}

uint16_t Recorder_GetLastRawMax(const Recorder_t *recorder)
{
  if (recorder == NULL) {
    return 0U;
  }
  return recorder->audio.last_raw_max;
}

uint16_t Recorder_GetLastCenterPeak(const Recorder_t *recorder)
{
  if (recorder == NULL) {
    return 0U;
  }
  return recorder->audio.last_center_peak;
}

uint16_t Recorder_GetLastAvgAbs(const Recorder_t *recorder)
{
  if (recorder == NULL) {
    return 0U;
  }
  return recorder->audio.last_avg_abs;
}

FRESULT Recorder_GetLastFresult(const Recorder_t *recorder)
{
  if (recorder == NULL) {
    return FR_INT_ERR;
  }
  return recorder->last_fresult;
}

RecorderErrorStage_t Recorder_GetLastErrorStage(const Recorder_t *recorder)
{
  if (recorder == NULL) {
    return RECORDER_STAGE_NONE;
  }
  return recorder->last_error_stage;
}

const char *Recorder_GetErrorStageString(RecorderErrorStage_t stage)
{
  switch (stage) {
    case RECORDER_STAGE_MOUNT:
      return "MOUNT";
    case RECORDER_STAGE_OPEN:
      return "OPEN";
    case RECORDER_STAGE_WRITE_HEADER:
      return "HEADER";
    case RECORDER_STAGE_PREALLOC:
      return "PREALLOC";
    case RECORDER_STAGE_START_TIM:
      return "TIM";
    case RECORDER_STAGE_START_ADC_DMA:
      return "ADC DMA";
    case RECORDER_STAGE_WRITE_DATA:
      return "WRITE";
    case RECORDER_STAGE_SYNC:
      return "SYNC";
    case RECORDER_STAGE_REWRITE_HEADER:
      return "REHEAD";
    case RECORDER_STAGE_CLOSE:
      return "CLOSE";
    case RECORDER_STAGE_UPLOAD:
      return "UPLOAD";
    case RECORDER_STAGE_NONE:
    default:
      return "NONE";
  }
}

