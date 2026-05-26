#ifndef BASE64_CODEC_H
#define BASE64_CODEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint16_t Base64_Encode(const uint8_t *input, uint16_t inputLength, char *output, uint16_t outputSize);
uint16_t Base64_EncodedLength(uint16_t inputLength);

#ifdef __cplusplus
}
#endif

#endif
