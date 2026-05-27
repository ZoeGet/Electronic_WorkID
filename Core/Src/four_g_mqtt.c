#include "four_g_mqtt.h"
#include "usart.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define FOUR_G_DEVICE_ID              "CARD0001"
#define FOUR_G_MQTT_HOST              "demo.freefly-ai.com"
#define FOUR_G_MQTT_PORT              1883
#define FOUR_G_MQTT_TOPIC             "gps/device/CARD0001/location"
#define FOUR_G_UART_TIMEOUT_MS        1000U
#define FOUR_G_RESPONSE_TIMEOUT_MS    800U
#define FOUR_G_PROMPT_TIMEOUT_MS      3000U
#define FOUR_G_CONNECT_OK_TIMEOUT_MS  5000U
#define FOUR_G_DISCONNECT_OK_TIMEOUT_MS  1000U
#define FOUR_G_PUBLISH_OK_TIMEOUT_MS  5000U
#define FOUR_G_COMMAND_DELAY_MS       1000U
#define FOUR_G_RX_BUFFER_SIZE         160U
#define FOUR_G_PAYLOAD_BUFFER_SIZE    128U
#define FOUR_G_COMMAND_BUFFER_SIZE    160U
#define FOUR_G_CONNECT_VALID_MS       120000U
#define FOUR_G_ACTIVE_PUBLISH_MS      60000U
#define FOUR_G_ASYNC_TX_BUFFER_SIZE   960U
#define FOUR_G_ASYNC_RX_BUFFER_SIZE   224U
#define FOUR_G_ASYNC_TX_CHUNK_BYTES   32U
#define FOUR_G_ASYNC_RX_POLL_BYTES    96U
#define FOUR_G_DMA_RX_BUFFER_SIZE     512U

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} FourGDateTime_t;

typedef enum {
    FOUR_G_ASYNC_OP_NONE = 0,
    FOUR_G_ASYNC_OP_CONNECT,
    FOUR_G_ASYNC_OP_DISCONNECT,
    FOUR_G_ASYNC_OP_PUBLISH,
    FOUR_G_ASYNC_OP_TIME
} FourGAsyncOp_t;

typedef enum {
    FOUR_G_ASYNC_STEP_IDLE = 0,
    FOUR_G_ASYNC_STEP_SEND_COMMAND,
    FOUR_G_ASYNC_STEP_WAIT_BEFORE_CONNECT,
    FOUR_G_ASYNC_STEP_SEND_CONNECT,
    FOUR_G_ASYNC_STEP_WAIT_PROMPT,
    FOUR_G_ASYNC_STEP_SEND_PAYLOAD,
    FOUR_G_ASYNC_STEP_WAIT_OK,
    FOUR_G_ASYNC_STEP_DONE,
    FOUR_G_ASYNC_STEP_ERROR
} FourGAsyncStep_t;

typedef struct {
    FourGAsyncOp_t op;
    FourGAsyncOp_t error_op;
    FourGAsyncStep_t step;
    FourGAsyncStep_t error_step;
    FourGMqttResult_t result;
    char tx[FOUR_G_ASYNC_TX_BUFFER_SIZE];
    uint16_t tx_len;
    uint16_t tx_pos;
    char payload[FOUR_G_ASYNC_TX_BUFFER_SIZE];
    uint16_t payload_len;
    uint16_t payload_pos;
    char response[FOUR_G_ASYNC_RX_BUFFER_SIZE];
    uint16_t response_len;
    char last_tx_summary[16];
    char last_rx_summary[16];
    uint32_t deadline_tick;
    uint32_t next_tx_tick;
    bool tx_dma_active;
    bool prompt_seen;
    uint64_t unix_time_ms;
    char timestamp[24];
} FourGAsyncContext_t;

static UART_HandleTypeDef *gFourGUart = NULL;
static uint8_t gFourGRxDmaBuffer[FOUR_G_DMA_RX_BUFFER_SIZE];
static uint16_t gFourGRxDmaReadPos = 0U;
static bool gFourGRxDmaStarted = false;
static uint32_t gLastConnectTick = 0U;
static uint32_t gLastPublishTick = 0U;
static bool gMqttConnected = false;
static FourGAsyncContext_t gFourGAsync;
static volatile bool gFourGTxDmaDone = false;
static volatile bool gFourGTxDmaError = false;

static FourGMqttResult_t FourG_SendRaw(const char *data);
static FourGMqttResult_t FourG_SendRawBytes(const char *data, uint16_t length);
static FourGMqttResult_t FourG_SendCommand(const char *command);
static void FourG_MQTT_Disconnect(void);
static FourGMqttResult_t FourG_ReadResponse(char *response, uint16_t responseSize, uint32_t timeoutMs);
static FourGMqttResult_t FourG_WaitForLineToken(const char *token, uint32_t timeoutMs);
static FourGMqttResult_t FourG_WaitForPrompt(uint32_t timeoutMs);
static void FourG_DrainInput(void);

static void FourG_AsyncResetResponse(void);
static void FourG_AsyncAppendRx(uint8_t byte);
static void FourG_AsyncPollRx(void);
static void FourG_UpdateTxSummary(const char *buffer, uint16_t length);
static bool FourG_AsyncSendChunk(char *buffer, uint16_t length, uint16_t *position);
static FourGMqttResult_t FourG_AsyncStartCommand(FourGAsyncOp_t op, const char *command);
static void FourG_AsyncSetWait(uint32_t timeout_ms);
static bool FourG_ResponseHasLineToken(const char *response, const char *token);
static FourGMqttResult_t FourG_GetNetworkTime(char *timestamp, uint16_t timestampSize);
static void FourG_StartDmaRx(void);
static void FourG_ProcessDmaRx(void);
static void FourG_ClearUartErrors(void);
static int FourG_RxBufferReadByte(uint8_t *byte);
static int FourG_FormatCoordinate(char *buffer, uint16_t bufferSize, float coordinate);
static void FourG_FormatTimestamp(char *timestamp, uint16_t timestampSize, const FourGDateTime_t *dateTime);
static uint8_t FourG_DaysInMonth(uint16_t year, uint8_t month);
static bool FourG_ParseNetworkTimeFromResponse(const char *response, FourGDateTime_t *dateTime);
static bool FourG_ParseNetworkTime(FourGDateTime_t *dateTime);
static uint64_t FourG_DateTimeToUnixMs(const FourGDateTime_t *dateTime);
void FourG_MQTT_Init(UART_HandleTypeDef *uart)
{
    gFourGUart = uart;
    gLastConnectTick = 0U;
    gLastPublishTick = 0U;
    gMqttConnected = false;
    memset(&gFourGAsync, 0, sizeof(gFourGAsync));
    gFourGTxDmaDone = false;
    gFourGTxDmaError = false;
    gFourGRxDmaReadPos = 0U;
    gFourGRxDmaStarted = false;
    FourG_StartDmaRx();
    gFourGAsync.step = FOUR_G_ASYNC_STEP_IDLE;
    gFourGAsync.error_step = FOUR_G_ASYNC_STEP_IDLE;
    gFourGAsync.error_op = FOUR_G_ASYNC_OP_NONE;
    gFourGAsync.result = FOUR_G_MQTT_OK;
}

FourGMqttResult_t FourG_MQTT_Connect(void)
{
    FourGMqttResult_t result;

    if (gFourGUart == NULL)
    {
        return FOUR_G_MQTT_ERROR;
    }

    FourG_DrainInput();
    result = FourG_SendCommand("AT+MQTTCONN=0,\"demo.freefly-ai.com\",1883,\"CARD0001\"");
    if (result == FOUR_G_MQTT_OK)
    {
        result = FourG_WaitForLineToken("OK", FOUR_G_CONNECT_OK_TIMEOUT_MS);
    }

    if (result != FOUR_G_MQTT_OK)
    {
        FourG_MQTT_Disconnect();
        FourG_DrainInput();

        result = FourG_SendCommand("AT+MQTTCONN=0,\"demo.freefly-ai.com\",1883,\"CARD0001\"");
        if (result == FOUR_G_MQTT_OK)
        {
            result = FourG_WaitForLineToken("OK", FOUR_G_CONNECT_OK_TIMEOUT_MS);
        }
    }

    if (result == FOUR_G_MQTT_OK)
    {
        gMqttConnected = true;
        gLastConnectTick = HAL_GetTick();
    }
    else
    {
        gMqttConnected = false;
    }

    return result;
}

FourGMqttResult_t FourG_MQTT_EnsureConnected(void)
{
    uint32_t now;

    if (gFourGUart == NULL)
    {
        return FOUR_G_MQTT_ERROR;
    }

    if ((gFourGAsync.step != FOUR_G_ASYNC_STEP_IDLE) &&
        (gFourGAsync.step != FOUR_G_ASYNC_STEP_DONE) &&
        (gFourGAsync.step != FOUR_G_ASYNC_STEP_ERROR))
    {
        return FOUR_G_MQTT_BUSY;
    }

    now = HAL_GetTick();
    if (gMqttConnected &&
        ((now - gLastConnectTick) < FOUR_G_CONNECT_VALID_MS) &&
        ((gLastPublishTick == 0U) || ((now - gLastPublishTick) < FOUR_G_ACTIVE_PUBLISH_MS)))
    {
        return FOUR_G_MQTT_OK;
    }

    return FourG_MQTT_Connect();
}

FourGMqttResult_t FourG_MQTT_PublishLocation(float latitude, float longitude)
{
    char timestamp[24];
    char payload[FOUR_G_PAYLOAD_BUFFER_SIZE];
    uint16_t payloadLength;
    FourGMqttResult_t result;

    if (gFourGUart == NULL)
    {
        return FOUR_G_MQTT_ERROR;
    }

    result = FourG_GetNetworkTime(timestamp, sizeof(timestamp));
    if (result != FOUR_G_MQTT_OK)
    {
        return result;
    }

    payloadLength = FourG_MQTT_BuildLocationPayload(payload, sizeof(payload), latitude, longitude, timestamp);
    if (payloadLength == 0U)
    {
        return FOUR_G_MQTT_ERROR;
    }

    result = FourG_MQTT_EnsureConnected();
    if (result != FOUR_G_MQTT_OK)
    {
        return result;
    }

    HAL_Delay(FOUR_G_COMMAND_DELAY_MS);

    return FourG_MQTT_PublishRaw(FOUR_G_MQTT_TOPIC, payload, payloadLength);
}

FourGMqttResult_t FourG_MQTT_PublishRaw(const char *topic, const char *payload, uint16_t payloadLength)
{
    char command[FOUR_G_COMMAND_BUFFER_SIZE];
    FourGMqttResult_t result;

    if ((topic == NULL) || (payload == NULL) || (payloadLength == 0U))
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    if (snprintf(command, sizeof(command), "AT+MQTTPUB=0,\"%s\",1,0,0,%u",
                 topic, (unsigned int)payloadLength) < 0)
    {
        return FOUR_G_MQTT_ERROR;
    }

    result = FourG_MQTT_EnsureConnected();
    if (result != FOUR_G_MQTT_OK)
    {
        return result;
    }

    FourG_DrainInput();

    result = FourG_SendCommand(command);
    if (result != FOUR_G_MQTT_OK)
    {
        FourG_MQTT_Disconnect();
        return result;
    }

    result = FourG_WaitForPrompt(FOUR_G_PROMPT_TIMEOUT_MS);
    if (result != FOUR_G_MQTT_OK)
    {
        FourG_MQTT_Disconnect();
        return result;
    }

    result = FourG_SendRawBytes(payload, payloadLength);
    if (result != FOUR_G_MQTT_OK)
    {
        FourG_MQTT_Disconnect();
        return result;
    }

    result = FourG_WaitForLineToken("OK", FOUR_G_PUBLISH_OK_TIMEOUT_MS);
    if (result == FOUR_G_MQTT_OK)
    {
        gLastPublishTick = HAL_GetTick();
    }
    else
    {
        FourG_MQTT_Disconnect();
    }

    return result;
}

FourGMqttResult_t FourG_MQTT_AsyncConnectStart(void)
{
    return FourG_AsyncStartCommand(FOUR_G_ASYNC_OP_CONNECT, "AT+MQTTDISC=0");
}

FourGMqttResult_t FourG_MQTT_AsyncEnsureConnectedStart(void)
{
    uint32_t now;

    if (gFourGUart == NULL)
    {
        return FOUR_G_MQTT_ERROR;
    }

    now = HAL_GetTick();
    if (gMqttConnected &&
        ((now - gLastConnectTick) < FOUR_G_CONNECT_VALID_MS) &&
        ((gLastPublishTick == 0U) || ((now - gLastPublishTick) < FOUR_G_ACTIVE_PUBLISH_MS)))
    {
        gFourGAsync.op = FOUR_G_ASYNC_OP_NONE;
        gFourGAsync.step = FOUR_G_ASYNC_STEP_DONE;
        gFourGAsync.result = FOUR_G_MQTT_OK;
        return FOUR_G_MQTT_OK;
    }

    return FourG_MQTT_AsyncConnectStart();
}

FourGMqttResult_t FourG_MQTT_AsyncDisconnectStart(void)
{
    return FourG_AsyncStartCommand(FOUR_G_ASYNC_OP_DISCONNECT, "AT+MQTTDISC=0");
}

FourGMqttResult_t FourG_MQTT_AsyncGetUnixTimeStart(void)
{
    return FourG_AsyncStartCommand(FOUR_G_ASYNC_OP_TIME, "AT+CCLK?");
}

FourGMqttResult_t FourG_MQTT_AsyncPublishRawStart(const char *topic, const char *payload, uint16_t payloadLength)
{
    int written;

    if ((topic == NULL) || (payload == NULL) || (payloadLength == 0U))
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    if ((gFourGAsync.step != FOUR_G_ASYNC_STEP_IDLE) &&
        (gFourGAsync.step != FOUR_G_ASYNC_STEP_DONE) &&
        (gFourGAsync.step != FOUR_G_ASYNC_STEP_ERROR))
    {
        return FOUR_G_MQTT_BUSY;
    }

    if (payloadLength >= sizeof(gFourGAsync.payload))
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    written = snprintf(gFourGAsync.tx, sizeof(gFourGAsync.tx),
                       "AT+MQTTPUB=0,\"%s\",1,0,0,%u\r\n",
                       topic, (unsigned int)payloadLength);
    if ((written < 0) || ((uint16_t)written >= sizeof(gFourGAsync.tx)))
    {
        return FOUR_G_MQTT_ERROR;
    }

    memcpy(gFourGAsync.payload, payload, payloadLength);
    gFourGAsync.payload[payloadLength] = '\0';
    gFourGAsync.payload_len = payloadLength;
    gFourGAsync.payload_pos = 0U;
    gFourGAsync.next_tx_tick = HAL_GetTick() + FOUR_G_COMMAND_DELAY_MS;
    gFourGAsync.tx_len = (uint16_t)written;
    gFourGAsync.tx_pos = 0U;
    gFourGAsync.tx_dma_active = false;
    gFourGAsync.op = FOUR_G_ASYNC_OP_PUBLISH;
    gFourGAsync.step = FOUR_G_ASYNC_STEP_SEND_COMMAND;
    gFourGAsync.error_step = FOUR_G_ASYNC_STEP_IDLE;
    gFourGAsync.error_op = FOUR_G_ASYNC_OP_NONE;
    gFourGAsync.result = FOUR_G_MQTT_BUSY;
    gFourGAsync.unix_time_ms = 0ULL;
    gFourGAsync.timestamp[0] = '\0';
    gFourGAsync.prompt_seen = false;
    FourG_AsyncResetResponse();
    FourG_DrainInput();

    return FOUR_G_MQTT_OK;
}

FourGMqttResult_t FourG_MQTT_AsyncPublishLocationStart(float latitude, float longitude, const char *timestamp)
{
    char payload[FOUR_G_PAYLOAD_BUFFER_SIZE];
    uint16_t payloadLength;

    payloadLength = FourG_MQTT_BuildLocationPayload(payload, sizeof(payload), latitude, longitude, timestamp);
    if (payloadLength == 0U)
    {
        return FOUR_G_MQTT_ERROR;
    }

    return FourG_MQTT_AsyncPublishRawStart(FOUR_G_MQTT_TOPIC, payload, payloadLength);
}

FourGMqttAsyncStatus_t FourG_MQTT_AsyncProcess(void)
{
    FourGDateTime_t dateTime;

    if (gFourGUart == NULL)
    {
        if (gFourGAsync.op != FOUR_G_ASYNC_OP_DISCONNECT) {
            gFourGAsync.error_step = gFourGAsync.step;
            gFourGAsync.error_op = gFourGAsync.op;
        }
        gFourGAsync.step = FOUR_G_ASYNC_STEP_ERROR;
        gFourGAsync.result = FOUR_G_MQTT_ERROR;
        return FOUR_G_MQTT_ASYNC_ERROR;
    }

    if ((gFourGAsync.step == FOUR_G_ASYNC_STEP_IDLE) ||
        (gFourGAsync.step == FOUR_G_ASYNC_STEP_DONE))
    {
        return (gFourGAsync.step == FOUR_G_ASYNC_STEP_DONE) ? FOUR_G_MQTT_ASYNC_DONE : FOUR_G_MQTT_ASYNC_IDLE;
    }

    if (gFourGAsync.step == FOUR_G_ASYNC_STEP_ERROR)
    {
        return FOUR_G_MQTT_ASYNC_ERROR;
    }

    FourG_AsyncPollRx();

    switch (gFourGAsync.step)
    {
        case FOUR_G_ASYNC_STEP_SEND_COMMAND:
            if (!FourG_AsyncSendChunk(gFourGAsync.tx, gFourGAsync.tx_len, &gFourGAsync.tx_pos))
            {
                return FOUR_G_MQTT_ASYNC_BUSY;
            }

            FourG_AsyncResetResponse();
            if (gFourGAsync.op == FOUR_G_ASYNC_OP_CONNECT)
            {
                gFourGAsync.step = FOUR_G_ASYNC_STEP_WAIT_BEFORE_CONNECT;
                FourG_AsyncSetWait(FOUR_G_DISCONNECT_OK_TIMEOUT_MS);
            }
            else if (gFourGAsync.op == FOUR_G_ASYNC_OP_PUBLISH)
            {
                gFourGAsync.step = FOUR_G_ASYNC_STEP_WAIT_PROMPT;
                FourG_AsyncSetWait(FOUR_G_PROMPT_TIMEOUT_MS);
            }
            else if (gFourGAsync.op == FOUR_G_ASYNC_OP_CONNECT)
            {
                gFourGAsync.step = FOUR_G_ASYNC_STEP_WAIT_OK;
                FourG_AsyncSetWait(FOUR_G_CONNECT_OK_TIMEOUT_MS);
            }
            else if (gFourGAsync.op == FOUR_G_ASYNC_OP_DISCONNECT)
            {
                gFourGAsync.step = FOUR_G_ASYNC_STEP_WAIT_OK;
                FourG_AsyncSetWait(FOUR_G_DISCONNECT_OK_TIMEOUT_MS);
            }
            else
            {
                gFourGAsync.step = FOUR_G_ASYNC_STEP_WAIT_OK;
                FourG_AsyncSetWait(FOUR_G_RESPONSE_TIMEOUT_MS);
            }
            break;

        case FOUR_G_ASYNC_STEP_WAIT_BEFORE_CONNECT:
            if (FourG_ResponseHasLineToken(gFourGAsync.response, "OK") ||
                FourG_ResponseHasLineToken(gFourGAsync.response, "ERROR") ||
                (strstr(gFourGAsync.response, "+CME ERROR") != NULL) ||
                (strstr(gFourGAsync.response, "+MQTTURC: \"conn\",0,2") != NULL) ||
                ((int32_t)(HAL_GetTick() - gFourGAsync.deadline_tick) >= 0))
            {
                int written = snprintf(gFourGAsync.tx, sizeof(gFourGAsync.tx),
                                       "AT+MQTTCONN=0,\"demo.freefly-ai.com\",1883,\"CARD0001\"\r\n");
                if ((written < 0) || ((uint16_t)written >= sizeof(gFourGAsync.tx)))
                {
                    if (gFourGAsync.op != FOUR_G_ASYNC_OP_DISCONNECT) {
                    gFourGAsync.error_step = gFourGAsync.step;
                    gFourGAsync.error_op = gFourGAsync.op;
                }
                gFourGAsync.step = FOUR_G_ASYNC_STEP_ERROR;
                    gFourGAsync.result = FOUR_G_MQTT_ERROR;
                    break;
                }
                gFourGAsync.tx_len = (uint16_t)written;
                gFourGAsync.tx_pos = 0U;
                gFourGAsync.next_tx_tick = HAL_GetTick() + FOUR_G_COMMAND_DELAY_MS;
                FourG_AsyncResetResponse();
                gFourGAsync.step = FOUR_G_ASYNC_STEP_SEND_CONNECT;
            }
            break;

        case FOUR_G_ASYNC_STEP_SEND_CONNECT:
            if (!FourG_AsyncSendChunk(gFourGAsync.tx, gFourGAsync.tx_len, &gFourGAsync.tx_pos))
            {
                return FOUR_G_MQTT_ASYNC_BUSY;
            }
            FourG_AsyncResetResponse();
            gFourGAsync.step = FOUR_G_ASYNC_STEP_WAIT_OK;
            FourG_AsyncSetWait(FOUR_G_CONNECT_OK_TIMEOUT_MS);
            break;

        case FOUR_G_ASYNC_STEP_WAIT_PROMPT:
            if (FourG_ResponseHasLineToken(gFourGAsync.response, "ERROR") ||
                (strstr(gFourGAsync.response, "+CME ERROR") != NULL))
            {
                if (gFourGAsync.op != FOUR_G_ASYNC_OP_DISCONNECT) {
                    gFourGAsync.error_step = gFourGAsync.step;
                    gFourGAsync.error_op = gFourGAsync.op;
                }
                gFourGAsync.step = FOUR_G_ASYNC_STEP_ERROR;
                gFourGAsync.result = FOUR_G_MQTT_ERROR;
                break;
            }
            if (gFourGAsync.prompt_seen)
            {
                gFourGAsync.step = FOUR_G_ASYNC_STEP_SEND_PAYLOAD;
                gFourGAsync.payload_pos = 0U;
                FourG_AsyncResetResponse();
                break;
            }
            if ((int32_t)(HAL_GetTick() - gFourGAsync.deadline_tick) >= 0)
            {
                if (gFourGAsync.op != FOUR_G_ASYNC_OP_DISCONNECT) {
                    gFourGAsync.error_step = gFourGAsync.step;
                    gFourGAsync.error_op = gFourGAsync.op;
                }
                gFourGAsync.step = FOUR_G_ASYNC_STEP_ERROR;
                gFourGAsync.result = FOUR_G_MQTT_TIMEOUT;
            }
            break;

        case FOUR_G_ASYNC_STEP_SEND_PAYLOAD:
            if (!FourG_AsyncSendChunk(gFourGAsync.payload, gFourGAsync.payload_len, &gFourGAsync.payload_pos))
            {
                return FOUR_G_MQTT_ASYNC_BUSY;
            }
            FourG_AsyncResetResponse();
            gFourGAsync.step = FOUR_G_ASYNC_STEP_WAIT_OK;
            FourG_AsyncSetWait(FOUR_G_PUBLISH_OK_TIMEOUT_MS);
            break;

        case FOUR_G_ASYNC_STEP_WAIT_OK:
            if (!FourG_ResponseHasLineToken(gFourGAsync.response, "OK") &&
                !((gFourGAsync.op == FOUR_G_ASYNC_OP_CONNECT) && (strstr(gFourGAsync.response, "OK") != NULL)) &&
                !((gFourGAsync.op == FOUR_G_ASYNC_OP_CONNECT) && (strstr(gFourGAsync.response, "+MQTTURC: \"conn\",0,0") != NULL)) &&
                !((gFourGAsync.op == FOUR_G_ASYNC_OP_CONNECT) && (strstr(gFourGAsync.response, "+CME ERROR: 607") != NULL)) &&
                (gFourGAsync.op != FOUR_G_ASYNC_OP_CONNECT) &&
                (FourG_ResponseHasLineToken(gFourGAsync.response, "ERROR") ||
                 (strstr(gFourGAsync.response, "+CME ERROR") != NULL)))
            {
                if (gFourGAsync.op != FOUR_G_ASYNC_OP_DISCONNECT) {
                    gFourGAsync.error_step = gFourGAsync.step;
                    gFourGAsync.error_op = gFourGAsync.op;
                }
                gFourGAsync.step = FOUR_G_ASYNC_STEP_ERROR;
                gFourGAsync.result = FOUR_G_MQTT_ERROR;
                break;
            }
            if (FourG_ResponseHasLineToken(gFourGAsync.response, "OK") ||
                ((gFourGAsync.op == FOUR_G_ASYNC_OP_CONNECT) && (strstr(gFourGAsync.response, "OK") != NULL)) ||
                ((gFourGAsync.op == FOUR_G_ASYNC_OP_CONNECT) && (strstr(gFourGAsync.response, "+MQTTURC: \"conn\",0,0") != NULL)) ||
                ((gFourGAsync.op == FOUR_G_ASYNC_OP_CONNECT) && (strstr(gFourGAsync.response, "607") != NULL) && (strstr(gFourGAsync.response, "CME ERROR") != NULL)))
            {
                if (gFourGAsync.op == FOUR_G_ASYNC_OP_TIME)
                {
                    memset(&dateTime, 0, sizeof(dateTime));
                    if (FourG_ParseNetworkTimeFromResponse(gFourGAsync.response, &dateTime))
                    {
                        gFourGAsync.unix_time_ms = FourG_DateTimeToUnixMs(&dateTime);
                        FourG_FormatTimestamp(gFourGAsync.timestamp, sizeof(gFourGAsync.timestamp), &dateTime);
                    }
                }

                if (gFourGAsync.op == FOUR_G_ASYNC_OP_CONNECT)
                {
                    gMqttConnected = true;
                    gLastConnectTick = HAL_GetTick();
                }
                else if (gFourGAsync.op == FOUR_G_ASYNC_OP_DISCONNECT)
                {
                    gMqttConnected = false;
                    gLastConnectTick = 0U;
                    gLastPublishTick = 0U;
                }
                else if (gFourGAsync.op == FOUR_G_ASYNC_OP_PUBLISH)
                {
                    gLastPublishTick = HAL_GetTick();
                }

                gFourGAsync.step = FOUR_G_ASYNC_STEP_DONE;
                gFourGAsync.result = FOUR_G_MQTT_OK;
                break;
            }
            if ((int32_t)(HAL_GetTick() - gFourGAsync.deadline_tick) >= 0)
            {
                if (gFourGAsync.op != FOUR_G_ASYNC_OP_DISCONNECT) {
                    gFourGAsync.error_step = gFourGAsync.step;
                    gFourGAsync.error_op = gFourGAsync.op;
                }
                gFourGAsync.step = FOUR_G_ASYNC_STEP_ERROR;
                gFourGAsync.result = FOUR_G_MQTT_TIMEOUT;
            }
            break;

        default:
            break;
    }

    if (gFourGAsync.step == FOUR_G_ASYNC_STEP_DONE)
    {
        return FOUR_G_MQTT_ASYNC_DONE;
    }

    if (gFourGAsync.step == FOUR_G_ASYNC_STEP_ERROR)
    {
        gMqttConnected = false;
        return FOUR_G_MQTT_ASYNC_ERROR;
    }

    return FOUR_G_MQTT_ASYNC_BUSY;
}

FourGMqttResult_t FourG_MQTT_AsyncGetResult(void)
{
    return gFourGAsync.result;
}

static const char *FourG_AsyncStepText(FourGAsyncStep_t step, FourGAsyncOp_t op)
{
    switch (step)
    {
        case FOUR_G_ASYNC_STEP_IDLE:
            return "IDLE";
        case FOUR_G_ASYNC_STEP_SEND_COMMAND:
            if (op == FOUR_G_ASYNC_OP_CONNECT) {
                return "DISC";
            }
            if (op == FOUR_G_ASYNC_OP_PUBLISH) {
                return "PUBCMD";
            }
            if (op == FOUR_G_ASYNC_OP_DISCONNECT) {
                return "DISCON";
            }
            return "CMD";
        case FOUR_G_ASYNC_STEP_WAIT_BEFORE_CONNECT:
            return "WAITDISC";
        case FOUR_G_ASYNC_STEP_SEND_CONNECT:
            return "CONN";
        case FOUR_G_ASYNC_STEP_WAIT_PROMPT:
            return "WAIT<";
        case FOUR_G_ASYNC_STEP_SEND_PAYLOAD:
            return "PAYLOAD";
        case FOUR_G_ASYNC_STEP_WAIT_OK:
            return "WAITOK";
        case FOUR_G_ASYNC_STEP_DONE:
            return "DONE";
        case FOUR_G_ASYNC_STEP_ERROR:
            return "ERROR";
        default:
            return "UNK";
    }
}

const char *FourG_MQTT_AsyncGetStepText(void)
{
    return FourG_AsyncStepText(gFourGAsync.step, gFourGAsync.op);
}

const char *FourG_MQTT_AsyncGetErrorStepText(void)
{
    return FourG_AsyncStepText(gFourGAsync.error_step, gFourGAsync.error_op);
}

const char *FourG_MQTT_AsyncGetErrorOpText(void)
{
    switch (gFourGAsync.error_op)
    {
        case FOUR_G_ASYNC_OP_CONNECT:
            return "CONN";
        case FOUR_G_ASYNC_OP_DISCONNECT:
            return "DISC";
        case FOUR_G_ASYNC_OP_PUBLISH:
            return "PUB";
        case FOUR_G_ASYNC_OP_TIME:
            return "TIME";
        case FOUR_G_ASYNC_OP_NONE:
        default:
            return "NONE";
    }
}uint64_t FourG_MQTT_AsyncGetUnixTimeMs(uint64_t fallback_ms)
{
    return (gFourGAsync.unix_time_ms == 0ULL) ? fallback_ms : gFourGAsync.unix_time_ms;
}

bool FourG_MQTT_AsyncGetTimestamp(char *timestamp, uint16_t timestampSize)
{
    if ((timestamp == NULL) || (timestampSize == 0U))
    {
        return false;
    }

    if (gFourGAsync.timestamp[0] == '\0')
    {
        timestamp[0] = '\0';
        return false;
    }

    (void)snprintf(timestamp, timestampSize, "%s", gFourGAsync.timestamp);
    return true;
}

uint16_t FourG_MQTT_BuildLocationPayload(char *buffer, uint16_t bufferSize,
                                         float latitude, float longitude,
                                         const char *timestamp)
{
    int written;
    char latitudeText[18];
    char longitudeText[18];

    if ((buffer == NULL) || (timestamp == NULL) || (bufferSize == 0U))
    {
        return 0U;
    }

    if ((FourG_FormatCoordinate(latitudeText, sizeof(latitudeText), latitude) < 0) ||
        (FourG_FormatCoordinate(longitudeText, sizeof(longitudeText), longitude) < 0))
    {
        return 0U;
    }

    written = snprintf(buffer, bufferSize,
                       "{\"device_id\":\"%s\",\"lat\":%s,\"lng\":%s,\"timestamp\":\"%s\"}",
                       FOUR_G_DEVICE_ID, latitudeText, longitudeText, timestamp);

    if ((written < 0) || ((uint16_t)written >= bufferSize))
    {
        return 0U;
    }

    return (uint16_t)written;
}

static FourGMqttResult_t FourG_SendRaw(const char *data)
{
    if (data == NULL)
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    return FourG_SendRawBytes(data, (uint16_t)strlen(data));
}

static FourGMqttResult_t FourG_SendRawBytes(const char *data, uint16_t length)
{
    if ((gFourGUart == NULL) || (data == NULL))
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    if (HAL_UART_Transmit(gFourGUart, (uint8_t *)data, length, FOUR_G_UART_TIMEOUT_MS) != HAL_OK)
    {
        return FOUR_G_MQTT_ERROR;
    }

    return FOUR_G_MQTT_OK;
}

static FourGMqttResult_t FourG_SendCommand(const char *command)
{
    char commandLine[FOUR_G_COMMAND_BUFFER_SIZE];
    int written;

    if (command == NULL)
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    written = snprintf(commandLine, sizeof(commandLine), "%s\r\n", command);
    if ((written < 0) || ((uint16_t)written >= sizeof(commandLine)))
    {
        return FOUR_G_MQTT_ERROR;
    }

    return FourG_SendRaw(commandLine);
}

static void FourG_MQTT_Disconnect(void)
{
    if (gFourGUart == NULL)
    {
        return;
    }

    (void)FourG_SendCommand("AT+MQTTDISC=0");
    (void)FourG_WaitForLineToken("OK", FOUR_G_DISCONNECT_OK_TIMEOUT_MS);
    gMqttConnected = false;
    gLastConnectTick = 0U;
    gLastPublishTick = 0U;
}

static FourGMqttResult_t FourG_ReadResponse(char *response, uint16_t responseSize, uint32_t timeoutMs)
{
    uint32_t startTick;
    uint16_t index = 0U;
    uint8_t byte;

    if ((gFourGUart == NULL) || (response == NULL) || (responseSize == 0U))
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    memset(response, 0, responseSize);
    startTick = HAL_GetTick();

    while ((HAL_GetTick() - startTick) < timeoutMs)
    {
        FourG_ProcessDmaRx();
        if (FourG_RxBufferReadByte(&byte) == 1)
        {
            if (index < (responseSize - 1U))
            {
                response[index++] = (char)byte;
                response[index] = '\0';
            }

            if ((strstr(response, "OK") != NULL) || (strstr(response, "ERROR") != NULL))
            {
                break;
            }
        }
    }

    if (index == 0U)
    {
        return FOUR_G_MQTT_TIMEOUT;
    }

    return (strstr(response, "ERROR") == NULL) ? FOUR_G_MQTT_OK : FOUR_G_MQTT_ERROR;
}

static bool FourG_ResponseHasLineToken(const char *response, const char *token)
{
    const char *cursor;
    size_t token_len;

    if ((response == NULL) || (token == NULL) || (token[0] == '\0'))
    {
        return false;
    }

    token_len = strlen(token);
    cursor = response;
    while ((cursor = strstr(cursor, token)) != NULL)
    {
        char before = (cursor == response) ? '\r' : cursor[-1];
        char after = cursor[token_len];
        bool line_start = (before == '\r') || (before == '\n');
        bool line_end = (after == '\0') || (after == '\r') || (after == '\n');

        if (line_start && line_end)
        {
            return true;
        }

        cursor += token_len;
    }

    return false;
}

static FourGMqttResult_t FourG_WaitForLineToken(const char *token, uint32_t timeoutMs)
{
    uint32_t startTick;
    uint16_t index = 0U;
    uint8_t byte;
    char response[FOUR_G_RX_BUFFER_SIZE];

    if ((gFourGUart == NULL) || (token == NULL) || (token[0] == '\0'))
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    memset(response, 0, sizeof(response));
    startTick = HAL_GetTick();

    while ((HAL_GetTick() - startTick) < timeoutMs)
    {
        FourG_ProcessDmaRx();
        if (FourG_RxBufferReadByte(&byte) == 1)
        {
            if (index < (sizeof(response) - 1U))
            {
                response[index++] = (char)byte;
                response[index] = '\0';
            }

            if (FourG_ResponseHasLineToken(response, "ERROR"))
            {
                return FOUR_G_MQTT_ERROR;
            }

            if (FourG_ResponseHasLineToken(response, token))
            {
                return FOUR_G_MQTT_OK;
            }
        }
    }

    return FOUR_G_MQTT_TIMEOUT;
}

static FourGMqttResult_t FourG_WaitForPrompt(uint32_t timeoutMs)
{
    uint32_t startTick;
    uint16_t index = 0U;
    uint8_t byte;
    char response[FOUR_G_RX_BUFFER_SIZE];

    if (gFourGUart == NULL)
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    memset(response, 0, sizeof(response));
    startTick = HAL_GetTick();
    while ((HAL_GetTick() - startTick) < timeoutMs)
    {
        FourG_ProcessDmaRx();
        if (FourG_RxBufferReadByte(&byte) == 1)
        {
            if ((byte == '>') || (byte == '<'))
            {
                return FOUR_G_MQTT_OK;
            }

            if (index < (sizeof(response) - 1U))
            {
                response[index++] = (char)byte;
                response[index] = '\0';
            }

            if (strstr(response, "ERROR") != NULL)
            {
                return FOUR_G_MQTT_ERROR;
            }
        }
    }

    return FOUR_G_MQTT_TIMEOUT;
}

static void FourG_DrainInput(void)
{
    uint16_t write_pos;

    if ((gFourGUart != NULL) && (gFourGUart->hdmarx != NULL) && gFourGRxDmaStarted)
    {
        write_pos = (uint16_t)(FOUR_G_DMA_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(gFourGUart->hdmarx));
        if (write_pos >= FOUR_G_DMA_RX_BUFFER_SIZE)
        {
            write_pos = 0U;
        }
        gFourGRxDmaReadPos = write_pos;
    }
    FourG_AsyncResetResponse();
}

static void FourG_AsyncResetResponse(void)
{
    memset(gFourGAsync.response, 0, sizeof(gFourGAsync.response));
    gFourGAsync.response_len = 0U;
    gFourGAsync.prompt_seen = false;
}

static void FourG_AsyncAppendRx(uint8_t byte)
{
    if ((byte == '>') || (byte == '<'))
    {
        gFourGAsync.prompt_seen = true;
    }

    if (gFourGAsync.response_len < (sizeof(gFourGAsync.response) - 1U))
    {
        gFourGAsync.response[gFourGAsync.response_len++] = (char)byte;
        gFourGAsync.response[gFourGAsync.response_len] = '\0';
    }
    else
    {
        memmove(&gFourGAsync.response[0], &gFourGAsync.response[1], sizeof(gFourGAsync.response) - 2U);
        gFourGAsync.response[sizeof(gFourGAsync.response) - 2U] = (char)byte;
        gFourGAsync.response[sizeof(gFourGAsync.response) - 1U] = '\0';
    }
}

static void FourG_ClearUartErrors(void)
{
    if (gFourGUart == NULL)
    {
        return;
    }

    __HAL_UART_CLEAR_OREFLAG(gFourGUart);
    __HAL_UART_CLEAR_FEFLAG(gFourGUart);
    __HAL_UART_CLEAR_NEFLAG(gFourGUart);
    __HAL_UART_CLEAR_PEFLAG(gFourGUart);
}

static void FourG_StartDmaRx(void)
{
    if ((gFourGUart == NULL) || gFourGRxDmaStarted)
    {
        return;
    }

    FourG_ClearUartErrors();
    memset(gFourGRxDmaBuffer, 0, sizeof(gFourGRxDmaBuffer));
    gFourGRxDmaReadPos = 0U;
    if (HAL_UART_Receive_DMA(gFourGUart, gFourGRxDmaBuffer, sizeof(gFourGRxDmaBuffer)) == HAL_OK)
    {
        gFourGRxDmaStarted = true;
    }
}

static int FourG_RxBufferReadByte(uint8_t *byte)
{
    uint16_t write_pos;

    if ((byte == NULL) || (gFourGUart == NULL) || (gFourGUart->hdmarx == NULL) || !gFourGRxDmaStarted)
    {
        return 0;
    }

    write_pos = (uint16_t)(FOUR_G_DMA_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(gFourGUart->hdmarx));
    if (write_pos >= FOUR_G_DMA_RX_BUFFER_SIZE)
    {
        write_pos = 0U;
    }

    if (gFourGRxDmaReadPos == write_pos)
    {
        return 0;
    }

    *byte = gFourGRxDmaBuffer[gFourGRxDmaReadPos];
    gFourGRxDmaReadPos = (uint16_t)((gFourGRxDmaReadPos + 1U) % FOUR_G_DMA_RX_BUFFER_SIZE);
    return 1;
}

static void FourG_ProcessDmaRx(void)
{
    uint8_t byte;
    uint16_t guard = FOUR_G_DMA_RX_BUFFER_SIZE;

    FourG_StartDmaRx();
    while ((guard-- > 0U) && (FourG_RxBufferReadByte(&byte) == 1))
    {
        FourG_AsyncAppendRx(byte);
    }
}

static void FourG_AsyncPollRx(void)
{
    FourG_ProcessDmaRx();
}

static void FourG_UpdateTxSummary(const char *buffer, uint16_t length)
{
    if (buffer == NULL)
    {
        return;
    }

    if (strstr(buffer, "AT+MQTTDISC") != NULL)
    {
        (void)snprintf(gFourGAsync.last_tx_summary, sizeof(gFourGAsync.last_tx_summary), "DISC");
    }
    else if (strstr(buffer, "AT+MQTTCONN") != NULL)
    {
        (void)snprintf(gFourGAsync.last_tx_summary, sizeof(gFourGAsync.last_tx_summary), "CONN");
    }
    else if (strstr(buffer, "AT+MQTTPUB") != NULL)
    {
        const char *payload_len = strrchr(buffer, ',');
        if (payload_len != NULL)
        {
            payload_len++;
            (void)snprintf(gFourGAsync.last_tx_summary, sizeof(gFourGAsync.last_tx_summary), "PUB%s", payload_len);
            for (uint8_t i = 0U; i < sizeof(gFourGAsync.last_tx_summary); i++)
            {
                if ((gFourGAsync.last_tx_summary[i] == '\r') || (gFourGAsync.last_tx_summary[i] == '\n'))
                {
                    gFourGAsync.last_tx_summary[i] = '\0';
                    break;
                }
            }
        }
        else
        {
            (void)snprintf(gFourGAsync.last_tx_summary, sizeof(gFourGAsync.last_tx_summary), "PUB");
        }
    }
    else if (strstr(buffer, "AT+CCLK") != NULL)
    {
        (void)snprintf(gFourGAsync.last_tx_summary, sizeof(gFourGAsync.last_tx_summary), "CCLK");
    }
    else
    {
        (void)snprintf(gFourGAsync.last_tx_summary, sizeof(gFourGAsync.last_tx_summary), "PAY%u", (unsigned int)length);
    }
}

static bool FourG_AsyncSendChunk(char *buffer, uint16_t length, uint16_t *position)
{
    HAL_StatusTypeDef status;

    if ((buffer == NULL) || (position == NULL) || (gFourGUart == NULL))
    {
        if (gFourGAsync.op != FOUR_G_ASYNC_OP_DISCONNECT) {
            gFourGAsync.error_step = gFourGAsync.step;
            gFourGAsync.error_op = gFourGAsync.op;
        }
        gFourGAsync.step = FOUR_G_ASYNC_STEP_ERROR;
        gFourGAsync.result = FOUR_G_MQTT_INVALID_PARAM;
        return false;
    }

    if (*position >= length)
    {
        gFourGAsync.tx_dma_active = false;
        return true;
    }

    if (gFourGAsync.tx_dma_active)
    {
        if (gFourGTxDmaDone)
        {
            gFourGTxDmaDone = false;
            gFourGAsync.tx_dma_active = false;
            *position = length;
            gFourGAsync.next_tx_tick = HAL_GetTick() + FOUR_G_COMMAND_DELAY_MS;
            return true;
        }

        if (gFourGTxDmaError || ((int32_t)(HAL_GetTick() - gFourGAsync.deadline_tick) >= 0))
        {
            (void)HAL_UART_AbortTransmit(gFourGUart);
            gFourGTxDmaError = false;
            gFourGAsync.tx_dma_active = false;
            if (gFourGAsync.op != FOUR_G_ASYNC_OP_DISCONNECT) {
                gFourGAsync.error_step = gFourGAsync.step;
                gFourGAsync.error_op = gFourGAsync.op;
            }
            gFourGAsync.step = FOUR_G_ASYNC_STEP_ERROR;
            gFourGAsync.result = FOUR_G_MQTT_TIMEOUT;
        }
        return false;
    }

    if ((int32_t)(HAL_GetTick() - gFourGAsync.next_tx_tick) < 0)
    {
        return false;
    }

    FourG_UpdateTxSummary(buffer, length);
    gFourGTxDmaDone = false;
    gFourGTxDmaError = false;
    status = HAL_UART_Transmit_DMA(gFourGUart, (uint8_t *)buffer, length);
    if (status == HAL_OK)
    {
        gFourGAsync.tx_dma_active = true;
        gFourGAsync.deadline_tick = HAL_GetTick() + FOUR_G_CONNECT_OK_TIMEOUT_MS;
        return false;
    }

    if (status != HAL_BUSY)
    {
        if (gFourGAsync.op != FOUR_G_ASYNC_OP_DISCONNECT) {
            gFourGAsync.error_step = gFourGAsync.step;
            gFourGAsync.error_op = gFourGAsync.op;
        }
        gFourGAsync.step = FOUR_G_ASYNC_STEP_ERROR;
        gFourGAsync.result = FOUR_G_MQTT_ERROR;
    }

    return false;
}

static FourGMqttResult_t FourG_AsyncStartCommand(FourGAsyncOp_t op, const char *command)
{
    int written;

    if ((gFourGUart == NULL) || (command == NULL))
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    if ((gFourGAsync.step != FOUR_G_ASYNC_STEP_IDLE) &&
        (gFourGAsync.step != FOUR_G_ASYNC_STEP_DONE) &&
        (gFourGAsync.step != FOUR_G_ASYNC_STEP_ERROR))
    {
        return FOUR_G_MQTT_BUSY;
    }

    written = snprintf(gFourGAsync.tx, sizeof(gFourGAsync.tx), "%s\r\n", command);
    if ((written < 0) || ((uint16_t)written >= sizeof(gFourGAsync.tx)))
    {
        return FOUR_G_MQTT_ERROR;
    }

    gFourGAsync.op = op;
    gFourGAsync.step = FOUR_G_ASYNC_STEP_SEND_COMMAND;
    if (op != FOUR_G_ASYNC_OP_DISCONNECT) {
        gFourGAsync.error_step = FOUR_G_ASYNC_STEP_IDLE;
        gFourGAsync.error_op = FOUR_G_ASYNC_OP_NONE;
    }
    gFourGAsync.result = FOUR_G_MQTT_BUSY;
    gFourGAsync.tx_len = (uint16_t)written;
    gFourGAsync.tx_pos = 0U;
    gFourGAsync.tx_dma_active = false;
    gFourGAsync.payload_len = 0U;
    gFourGAsync.payload_pos = 0U;
    gFourGAsync.next_tx_tick = HAL_GetTick() + FOUR_G_COMMAND_DELAY_MS;
    gFourGAsync.unix_time_ms = 0ULL;
    gFourGAsync.timestamp[0] = '\0';
    FourG_AsyncResetResponse();
    FourG_DrainInput();

    return FOUR_G_MQTT_OK;
}

static void FourG_AsyncSetWait(uint32_t timeout_ms)
{
    gFourGAsync.deadline_tick = HAL_GetTick() + timeout_ms;
}

static FourGMqttResult_t FourG_GetNetworkTime(char *timestamp, uint16_t timestampSize)
{
    FourGDateTime_t dateTime;

    if ((timestamp == NULL) || (timestampSize < 20U))
    {
        return FOUR_G_MQTT_INVALID_PARAM;
    }

    if (!FourG_ParseNetworkTime(&dateTime))
    {
        return FOUR_G_MQTT_ERROR;
    }

    FourG_FormatTimestamp(timestamp, timestampSize, &dateTime);
    return FOUR_G_MQTT_OK;
}

static int FourG_FormatCoordinate(char *buffer, uint16_t bufferSize, float coordinate)
{
    int32_t scaled;
    int32_t integerPart;
    int32_t fractionalPart;

    if ((buffer == NULL) || (bufferSize == 0U))
    {
        return -1;
    }

    scaled = (int32_t)((coordinate >= 0.0f) ? ((coordinate * 100000.0f) + 0.5f) : ((coordinate * 100000.0f) - 0.5f));
    integerPart = scaled / 100000;
    fractionalPart = scaled % 100000;
    if (fractionalPart < 0)
    {
        fractionalPart = -fractionalPart;
    }

    return snprintf(buffer, bufferSize, "%ld.%05ld", (long)integerPart, (long)fractionalPart);
}

static void FourG_FormatTimestamp(char *timestamp, uint16_t timestampSize, const FourGDateTime_t *dateTime)
{
    if ((timestamp == NULL) || (dateTime == NULL) || (timestampSize == 0U))
    {
        return;
    }

    (void)snprintf(timestamp, timestampSize, "20%02u-%02u-%02uT%02u:%02u:%02u",
                   dateTime->year, dateTime->month, dateTime->day,
                   dateTime->hour, dateTime->minute, dateTime->second);
}

static bool FourG_ParseNetworkTimeFromResponse(const char *response, FourGDateTime_t *dateTime)
{
    const char *timeStart;
    int scanned;

    if ((response == NULL) || (dateTime == NULL))
    {
        return false;
    }

    timeStart = strstr(response, "+CCLK: \"");
    if (timeStart == NULL)
    {
        return false;
    }

    timeStart += strlen("+CCLK: \"");
    scanned = sscanf(timeStart, "%2hhu/%2hhu/%2hhu,%2hhu:%2hhu:%2hhu",
                     &dateTime->year, &dateTime->month, &dateTime->day,
                     &dateTime->hour, &dateTime->minute, &dateTime->second);

    return scanned == 6;
}

static bool FourG_ParseNetworkTime(FourGDateTime_t *dateTime)
{
    char response[FOUR_G_RX_BUFFER_SIZE];
    FourGMqttResult_t result;

    if (dateTime == NULL)
    {
        return false;
    }

    FourG_DrainInput();

    result = FourG_SendCommand("AT+CCLK?");
    if (result != FOUR_G_MQTT_OK)
    {
        return false;
    }

    result = FourG_ReadResponse(response, sizeof(response), FOUR_G_RESPONSE_TIMEOUT_MS);
    if (result != FOUR_G_MQTT_OK)
    {
        return false;
    }

    return FourG_ParseNetworkTimeFromResponse(response, dateTime);
}

static uint8_t FourG_DaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    if ((month < 1U) || (month > 12U))
    {
        return 30U;
    }

    if ((month == 2U) && (((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U))))
    {
        return 29U;
    }

    return days[month - 1U];
}

static uint64_t FourG_DateTimeToUnixMs(const FourGDateTime_t *dateTime)
{
    uint16_t full_year;
    uint64_t days = 0ULL;

    if (dateTime == NULL)
    {
        return 0ULL;
    }

    full_year = (uint16_t)(2000U + dateTime->year);
    if ((full_year < 1970U) || (dateTime->month < 1U) || (dateTime->month > 12U) || (dateTime->day < 1U))
    {
        return 0ULL;
    }

    for (uint16_t year = 1970U; year < full_year; year++)
    {
        days += (((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U))) ? 366ULL : 365ULL;
    }

    for (uint8_t month = 1U; month < dateTime->month; month++)
    {
        days += FourG_DaysInMonth(full_year, month);
    }

    days += (uint64_t)(dateTime->day - 1U);
    return (((days * 24ULL + dateTime->hour) * 60ULL + dateTime->minute) * 60ULL + dateTime->second) * 1000ULL;
}

uint64_t FourG_MQTT_GetUnixTimeMs(uint64_t fallback_ms)
{
    FourGDateTime_t dateTime;
    uint64_t timestamp_ms;

    if (!FourG_ParseNetworkTime(&dateTime))
    {
        return fallback_ms;
    }

    timestamp_ms = FourG_DateTimeToUnixMs(&dateTime);
    return (timestamp_ms == 0ULL) ? fallback_ms : timestamp_ms;
}

uint16_t FourG_MQTT_GetDmaWritePos(void)
{
    if ((gFourGUart == NULL) || (gFourGUart->hdmarx == NULL) || !gFourGRxDmaStarted)
    {
        return 0xFFFFU;
    }
    return (uint16_t)(FOUR_G_DMA_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(gFourGUart->hdmarx));
}

const char *FourG_MQTT_GetTxSummary(void)
{
    return (gFourGAsync.last_tx_summary[0] == '\0') ? "NONE" : gFourGAsync.last_tx_summary;
}

const char *FourG_MQTT_GetRxSummary(void)
{
    if ((strstr(gFourGAsync.response, "607") != NULL) &&
        ((strstr(gFourGAsync.response, "+CME ERROR") != NULL) ||
         (strstr(gFourGAsync.response, "CME ERROR") != NULL)))
    {
        return "CME607";
    }
    if (strstr(gFourGAsync.response, "+MQTTURC: \"conn\",0,0") != NULL)
    {
        return "CONN0";
    }
    if (strstr(gFourGAsync.response, "+MQTTURC: \"conn\",0,2") != NULL)
    {
        return "DISC2";
    }
    if (strstr(gFourGAsync.response, "OK") != NULL)
    {
        return "OK";
    }
    if ((strstr(gFourGAsync.response, "+CME ERROR") != NULL) ||
        (strstr(gFourGAsync.response, "CME ERROR") != NULL))
    {
        const char *code = strstr(gFourGAsync.response, "ERROR");
        if (code != NULL)
        {
            code = strchr(code, ':');
            if (code != NULL)
            {
                static char cme_summary[12];
                code++;
                while (*code == ' ')
                {
                    code++;
                }
                (void)snprintf(cme_summary, sizeof(cme_summary), "CME:%s", code);
                cme_summary[sizeof(cme_summary) - 1U] = '\0';
                for (uint8_t i = 4U; i < (sizeof(cme_summary) - 1U); i++)
                {
                    if ((cme_summary[i] == '\r') || (cme_summary[i] == '\n') || (cme_summary[i] == '\0'))
                    {
                        cme_summary[i] = '\0';
                        break;
                    }
                }
                return cme_summary;
            }
        }
        return "CME";
    }
    if (strstr(gFourGAsync.response, "ERROR") != NULL)
    {
        return "ERR";
    }
    return "NONE";
}

bool FourG_MQTT_IsDmaStarted(void)
{
    return gFourGRxDmaStarted;
}

const uint8_t *FourG_MQTT_GetDmaBuffer(void)
{
    return gFourGRxDmaBuffer;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == gFourGUart)
    {
        gFourGTxDmaDone = true;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == gFourGUart)
    {
        gFourGTxDmaError = true;
        FourG_ClearUartErrors();
        FourG_StartDmaRx();
    }
}
