#include "audio_buffer.h"
#include <string.h>

#define AUDIO_ADC_BIAS_INIT       2048
#define AUDIO_DC_TRACK_SHIFT      10
#define AUDIO_SAMPLE_GAIN_SHIFT   4

static void AudioBuffer_ConvertToPcm(AudioBuffer_t *ctx, const uint16_t *src, uint8_t *dst, uint32_t samples)
{
  uint16_t raw_min = 0xFFFFU;
  uint16_t raw_max = 0U;
  uint32_t abs_sum = 0U;
  uint32_t peak_abs = 0U;

  for (uint32_t i = 0; i < samples; i++) {
    int32_t raw = (int32_t)src[i];

    if (src[i] < raw_min) {
      raw_min = src[i];
    }
    if (src[i] > raw_max) {
      raw_max = src[i];
    }

    ctx->dc_estimate_q8 += ((raw << 8) - ctx->dc_estimate_q8) >> AUDIO_DC_TRACK_SHIFT;

    int32_t centered = raw - (ctx->dc_estimate_q8 >> 8);
    int32_t filtered = (centered + ctx->prev_centered) / 2;
    ctx->prev_centered = centered;

    uint32_t abs_filtered = (filtered >= 0) ? (uint32_t)filtered : (uint32_t)(-filtered);
    abs_sum += abs_filtered;
    if (abs_filtered > peak_abs) {
      peak_abs = abs_filtered;
    }

    int32_t scaled = filtered << AUDIO_SAMPLE_GAIN_SHIFT;
    if (scaled > 32767) {
      scaled = 32767;
    } else if (scaled < -32768) {
      scaled = -32768;
    }

    int16_t pcm = (int16_t)scaled;
    dst[(i * 2U)] = (uint8_t)(pcm & 0xFF);
    dst[(i * 2U) + 1U] = (uint8_t)(((uint16_t)pcm >> 8) & 0xFF);
  }

  ctx->last_raw_min = raw_min;
  ctx->last_raw_max = raw_max;
  ctx->last_center_peak = (uint16_t)((peak_abs > 65535U) ? 65535U : peak_abs);
  ctx->last_avg_abs = (uint16_t)(abs_sum / samples);
}

void AudioBuffer_Init(AudioBuffer_t *ctx)
{
  if (ctx == NULL) {
    return;
  }

  memset(ctx, 0, sizeof(*ctx));
  ctx->dc_estimate_q8 = (int32_t)(AUDIO_ADC_BIAS_INIT << 8);
}

static void AudioBuffer_PushHalf(AudioBuffer_t *ctx, const uint16_t *half_ptr)
{
  uint8_t write_index;

  if ((ctx == NULL) || (half_ptr == NULL)) {
    return;
  }

  if (ctx->ready_count >= AUDIO_QUEUE_BLOCKS) {
    ctx->overrun_count++;
    return;
  }

  write_index = ctx->write_index;
  AudioBuffer_ConvertToPcm(ctx, half_ptr, ctx->blocks[write_index], AUDIO_HALF_SAMPLES);

  ctx->write_index = (uint8_t)((write_index + 1U) % AUDIO_QUEUE_BLOCKS);
  ctx->ready_count++;
}

void AudioBuffer_OnDmaHalfComplete(AudioBuffer_t *ctx)
{
  AudioBuffer_PushHalf(ctx, &ctx->dma_buffer[0]);
}

void AudioBuffer_OnDmaFullComplete(AudioBuffer_t *ctx)
{
  AudioBuffer_PushHalf(ctx, &ctx->dma_buffer[AUDIO_HALF_SAMPLES]);
}

bool AudioBuffer_GetReadyBlock(AudioBuffer_t *ctx, uint8_t **block, uint32_t *size)
{
  uint8_t read_index;

  if ((ctx == NULL) || (block == NULL) || (size == NULL) || (ctx->ready_count == 0U)) {
    return false;
  }

  read_index = ctx->read_index;
  *block = ctx->blocks[read_index];
  *size = AUDIO_BLOCK_BYTES;

  ctx->read_index = (uint8_t)((read_index + 1U) % AUDIO_QUEUE_BLOCKS);
  ctx->ready_count--;
  return true;
}
