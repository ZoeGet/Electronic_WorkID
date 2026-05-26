#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define AUDIO_DMA_SAMPLES      512U
#define AUDIO_HALF_SAMPLES     (AUDIO_DMA_SAMPLES / 2U)
#define AUDIO_PCM_SAMPLES      AUDIO_HALF_SAMPLES
#define AUDIO_BLOCK_BYTES      (AUDIO_PCM_SAMPLES * 2U)
#define AUDIO_QUEUE_BLOCKS     8U

typedef struct {
  uint16_t dma_buffer[AUDIO_DMA_SAMPLES];
  uint8_t blocks[AUDIO_QUEUE_BLOCKS][AUDIO_BLOCK_BYTES];
  volatile uint8_t ready_count;
  volatile uint32_t overrun_count;
  uint8_t write_index;
  uint8_t read_index;
  int32_t dc_estimate_q8;
  int32_t prev_centered;
  volatile uint16_t last_raw_min;
  volatile uint16_t last_raw_max;
  volatile uint16_t last_center_peak;
  volatile uint16_t last_avg_abs;
} AudioBuffer_t;

void AudioBuffer_Init(AudioBuffer_t *ctx);
void AudioBuffer_OnDmaHalfComplete(AudioBuffer_t *ctx);
void AudioBuffer_OnDmaFullComplete(AudioBuffer_t *ctx);
bool AudioBuffer_GetReadyBlock(AudioBuffer_t *ctx, uint8_t **block, uint32_t *size);

#ifdef __cplusplus
}
#endif

#endif
