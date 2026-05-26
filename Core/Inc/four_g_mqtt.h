#ifndef __FOUR_G_MQTT_H__
#define __FOUR_G_MQTT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"
#include <stdint.h>

typedef enum {
    FOUR_G_MQTT_OK = 0,
    FOUR_G_MQTT_ERROR,
    FOUR_G_MQTT_INVALID_PARAM,
    FOUR_G_MQTT_TIMEOUT,
    FOUR_G_MQTT_BUSY,
    FOUR_G_MQTT_TOPIC_MISMATCH
} FourGMqttResult_t;

void FourG_MQTT_Init(UART_HandleTypeDef *uart);
FourGMqttResult_t FourG_MQTT_EnsureConnected(void);
FourGMqttResult_t FourG_MQTT_PublishRaw(const char *topic, const char *payload, uint16_t payloadLength);
FourGMqttResult_t FourG_MQTT_PublishLocation(float latitude, float longitude);
uint16_t FourG_MQTT_BuildLocationPayload(char *buffer, uint16_t bufferSize,
                                         float latitude, float longitude,
                                         const char *timestamp);
uint64_t FourG_MQTT_GetUnixTimeMs(uint64_t fallback_ms);

#ifdef __cplusplus
}
#endif

#endif /* __FOUR_G_MQTT_H__ */
