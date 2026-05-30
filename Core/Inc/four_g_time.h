#ifndef __FOUR_G_TIME_H__
#define __FOUR_G_TIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} FourGTime_t;

typedef enum {
  FOUR_G_TIME_OK = 0,
  FOUR_G_TIME_INVALID_PARAM,
  FOUR_G_TIME_UART_ERROR,
  FOUR_G_TIME_TIMEOUT,
  FOUR_G_TIME_PARSE_ERROR,
  FOUR_G_TIME_INVALID_TIME
} FourGTimeResult_t;

typedef enum {
  FOUR_G_TIME_ASYNC_IDLE = 0,
  FOUR_G_TIME_ASYNC_BUSY,
  FOUR_G_TIME_ASYNC_DONE,
  FOUR_G_TIME_ASYNC_ERROR
} FourGTimeAsyncStatus_t;

void FourG_Time_Init(UART_HandleTypeDef *huart);
FourGTimeResult_t FourG_Time_Get(FourGTime_t *time, uint32_t timeout_ms);
FourGTimeResult_t FourG_Time_AsyncStart(uint32_t timeout_ms);
FourGTimeAsyncStatus_t FourG_Time_AsyncProcess(FourGTime_t *time, FourGTimeResult_t *result);
void FourG_Time_AsyncCancel(void);
const char *FourG_Time_ResultText(FourGTimeResult_t result);

#ifdef __cplusplus
}
#endif

#endif /* __FOUR_G_TIME_H__ */
