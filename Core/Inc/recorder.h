#ifndef RECORDER_H
#define RECORDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "audio_buffer.h"
#include "ff.h"
#include <stdbool.h>
#include <stdint.h>

#define REC_SAMPLE_RATE       16000U
#define REC_BITS_PER_SAMPLE   16U
#define REC_CHANNELS          1U

typedef enum {
  RECORDER_OK = 0,
  RECORDER_ERR_STATE,
  RECORDER_ERR_FATFS,
  RECORDER_ERR_IO
} RecorderResult_t;

typedef enum {
  RECORDER_STATE_IDLE = 0,
  RECORDER_STATE_READY,
  RECORDER_STATE_RECORDING,
  RECORDER_STATE_ERROR
} RecorderState_t;

typedef enum {
  RECORDER_STAGE_NONE = 0,
  RECORDER_STAGE_MOUNT,
  RECORDER_STAGE_OPEN,
  RECORDER_STAGE_WRITE_HEADER,
  RECORDER_STAGE_PREALLOC,
  RECORDER_STAGE_START_TIM,
  RECORDER_STAGE_START_ADC_DMA,
  RECORDER_STAGE_WRITE_DATA,
  RECORDER_STAGE_SYNC,
  RECORDER_STAGE_REWRITE_HEADER,
  RECORDER_STAGE_CLOSE,
  RECORDER_STAGE_UPLOAD
} RecorderErrorStage_t;

typedef struct {
  AudioBuffer_t audio;
  FIL file;
  uint32_t pcm_bytes_written;
  uint32_t sync_acc_bytes;
  RecorderState_t state;
  FRESULT last_fresult;
  RecorderErrorStage_t last_error_stage;
} Recorder_t;

RecorderResult_t Recorder_Init(Recorder_t *recorder);
RecorderResult_t Recorder_Start(Recorder_t *recorder, const char *filename, uint32_t expected_pcm_bytes);
RecorderResult_t Recorder_Process(Recorder_t *recorder);
RecorderResult_t Recorder_SwitchFile(Recorder_t *recorder, const char *next_filename);
RecorderResult_t Recorder_Stop(Recorder_t *recorder);
void Recorder_OnAdcHalfComplete(Recorder_t *recorder);
void Recorder_OnAdcFullComplete(Recorder_t *recorder);
RecorderState_t Recorder_GetState(const Recorder_t *recorder);
uint32_t Recorder_GetOverrunCount(const Recorder_t *recorder);
uint16_t Recorder_GetLastRawMin(const Recorder_t *recorder);
uint16_t Recorder_GetLastRawMax(const Recorder_t *recorder);
uint16_t Recorder_GetLastCenterPeak(const Recorder_t *recorder);
uint16_t Recorder_GetLastAvgAbs(const Recorder_t *recorder);
FRESULT Recorder_GetLastFresult(const Recorder_t *recorder);
RecorderErrorStage_t Recorder_GetLastErrorStage(const Recorder_t *recorder);
const char *Recorder_GetErrorStageString(RecorderErrorStage_t stage);

#ifdef __cplusplus
}
#endif

#endif
