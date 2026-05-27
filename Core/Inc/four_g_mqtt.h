#ifndef __FOUR_G_MQTT_H__
#define __FOUR_G_MQTT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FOUR_G_MQTT_OK = 0,
    FOUR_G_MQTT_ERROR,
    FOUR_G_MQTT_INVALID_PARAM,
    FOUR_G_MQTT_TIMEOUT,
    FOUR_G_MQTT_BUSY,
    FOUR_G_MQTT_TOPIC_MISMATCH
} FourGMqttResult_t;

typedef enum {
    FOUR_G_MQTT_ASYNC_IDLE = 0,
    FOUR_G_MQTT_ASYNC_BUSY,
    FOUR_G_MQTT_ASYNC_DONE,
    FOUR_G_MQTT_ASYNC_ERROR
} FourGMqttAsyncStatus_t;

void FourG_MQTT_Init(UART_HandleTypeDef *uart);
FourGMqttResult_t FourG_MQTT_Connect(void);
FourGMqttResult_t FourG_MQTT_EnsureConnected(void);
FourGMqttResult_t FourG_MQTT_PublishRaw(const char *topic, const char *payload, uint16_t payloadLength);
FourGMqttResult_t FourG_MQTT_PublishLocation(float latitude, float longitude);
uint16_t FourG_MQTT_BuildLocationPayload(char *buffer, uint16_t bufferSize,
                                         float latitude, float longitude,
                                         const char *timestamp);
uint64_t FourG_MQTT_GetUnixTimeMs(uint64_t fallback_ms);
FourGMqttResult_t FourG_MQTT_AsyncConnectStart(void);
FourGMqttResult_t FourG_MQTT_AsyncEnsureConnectedStart(void);
FourGMqttResult_t FourG_MQTT_AsyncDisconnectStart(void);
FourGMqttResult_t FourG_MQTT_AsyncPublishRawStart(const char *topic, const char *payload, uint16_t payloadLength);
FourGMqttResult_t FourG_MQTT_AsyncPublishLocationStart(float latitude, float longitude, const char *timestamp);
FourGMqttResult_t FourG_MQTT_AsyncGetUnixTimeStart(void);
FourGMqttAsyncStatus_t FourG_MQTT_AsyncProcess(void);
FourGMqttResult_t FourG_MQTT_AsyncGetResult(void);
uint64_t FourG_MQTT_AsyncGetUnixTimeMs(uint64_t fallback_ms);
bool FourG_MQTT_AsyncGetTimestamp(char *timestamp, uint16_t timestampSize);
const char *FourG_MQTT_AsyncGetStepText(void);
const char *FourG_MQTT_AsyncGetErrorStepText(void);
const char *FourG_MQTT_AsyncGetErrorOpText(void);
const char *FourG_MQTT_GetTxSummary(void);
const char *FourG_MQTT_GetRxSummary(void);
uint16_t FourG_MQTT_GetDmaWritePos(void);
bool FourG_MQTT_IsDmaStarted(void);
const uint8_t *FourG_MQTT_GetDmaBuffer(void);

#ifdef __cplusplus
}
#endif

#endif /* __FOUR_G_MQTT_H__ */
