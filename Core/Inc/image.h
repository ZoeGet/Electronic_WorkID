#ifndef __IMAGE_H__
#define __IMAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define WORK_CARD_IMAGE_WIDTH     164U
#define WORK_CARD_IMAGE_HEIGHT    50U
#define WORK_CARD_IMAGE_HEADER_SIZE 8U
#define WORK_CARD_IMAGE_TOTAL_SIZE 16408U

extern const unsigned char gImage_image[];
extern const unsigned char gImage_image2[];

#ifdef __cplusplus
}
#endif

#endif /* __IMAGE_H__ */
