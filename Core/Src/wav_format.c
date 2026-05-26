#include "wav_format.h"
#include <string.h>

static void WAV_WriteLe16(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void WAV_WriteLe32(uint8_t *dst, uint32_t value)
{
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)((value >> 8) & 0xFFU);
  dst[2] = (uint8_t)((value >> 16) & 0xFFU);
  dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

void WAV_BuildHeader(uint8_t *header, uint32_t sample_rate, uint16_t bits_per_sample,
                     uint16_t channels, uint32_t pcm_bytes)
{
  uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8U);
  uint16_t block_align = (uint16_t)(channels * (bits_per_sample / 8U));
  uint32_t riff_size = 36U + pcm_bytes;

  memset(header, 0, WAV_HEADER_SIZE);
  memcpy(&header[0], "RIFF", 4);
  WAV_WriteLe32(&header[4], riff_size);
  memcpy(&header[8], "WAVE", 4);
  memcpy(&header[12], "fmt ", 4);
  WAV_WriteLe32(&header[16], 16U);
  WAV_WriteLe16(&header[20], 1U);
  WAV_WriteLe16(&header[22], channels);
  WAV_WriteLe32(&header[24], sample_rate);
  WAV_WriteLe32(&header[28], byte_rate);
  WAV_WriteLe16(&header[32], block_align);
  WAV_WriteLe16(&header[34], bits_per_sample);
  memcpy(&header[36], "data", 4);
  WAV_WriteLe32(&header[40], pcm_bytes);
}
