#ifndef WAV_FORMAT_H
#define WAV_FORMAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define WAV_HEADER_SIZE 44U

void WAV_BuildHeader(uint8_t *header, uint32_t sample_rate, uint16_t bits_per_sample,
                     uint16_t channels, uint32_t pcm_bytes);

#ifdef __cplusplus
}
#endif

#endif
