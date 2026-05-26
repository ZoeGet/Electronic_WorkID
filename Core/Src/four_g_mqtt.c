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
#define FOUR_G_PUBLISH_OK_TIMEOUT_MS  5000U
#define FOUR_G_COMMAND_DELAY_MS       20U
#define FOUR_G_RX_BUFFER_SIZE         160U
#define FOUR_G_PAYLOAD_BUFFER_SIZE    128U
#define FOUR_G_COMMAND_BUFFER_SIZE    160U
#define FOUR_G_CONNECT_VALID_MS       120000U
#define FOUR_G_ACTIVE_PUBLISH_MS      60000U

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} FourGDateTime_t;

static UART_HandleTypeDef *gFourGUart = NULL;
static uint32_t gLastConnectTick = 0U;
static uint32_t gLastPublishTick = 0U;
static bool gMqttConnected = false;

static FourGMqttResult_t FourG_SendRaw(const char *data);
static FourGMqttResult_t FourG_SendRawBytes(const char *data, uint16_t length);
static FourGMqttResult_t FourG_SendCommand(const char *command);
static FourGMqttResult_t FourG_ReadResponse(char *response, uint16_t responseSize, uint32_t timeoutMs);
static FourGMqttResult_t FourG_WaitForToken(const char *token, uint32_t timeoutMs);
static FourGMqttResult_t FourG_WaitForPrompt(uint32_t timeoutMs);
static void FourG_DrainInput(void);
static FourGMqttResult_t FourG_GetNetworkTime(char *timestamp, uint16_t timestampSize);
static int FourG_FormatCoordinate(char *buffer, uint16_t bufferSize, float coordinate);
static void FourG_FormatTimestamp(char *timestamp, uint16_t timestampSize, const FourGDateTime_t *dateTime);
static uint8_t FourG_DaysInMonth(uint16_t year, uint8_t month);
static bool FourG_ParseNetworkTime(FourGDateTime_t *dateTime);
static uint64_t FourG_DateTimeToUnixMs(const FourGDateTime_t *dateTime);

void FourG_MQTT_Init(UART_HandleTypeDef *uart)
{
    gFourGUart = uart;
    gLastConnectTick = 0U;
    gLastPublishTick = 0U;
    gMqttConnected = false;
}

FourGMqttResult_t FourG_MQTT_EnsureConnected(void)
{
    uint32_t now;
    FourGMqttResult_t result;

    if (gFourGUart == NULL)
    {
        return FOUR_G_MQTT_ERROR;
    }

    now = HAL_GetTick();
    if (gMqttConnected &&
        ((now - gLastConnectTick) < FOUR_G_CONNECT_VALID_MS) &&
        ((gLastPublishTick == 0U) || ((now - gLastPublishTick) < FOUR_G_ACTIVE_PUBLISH_MS)))
    {
        return FOUR_G_MQTT_OK;
    }

    result = FourG_SendCommand("AT+MQTTCONN=0,\"demo.freefly-ai.com\",1883,\"CARD0001\"");
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
        return result;
    }

    result = FourG_WaitForPrompt(FOUR_G_PROMPT_TIMEOUT_MS);
    if (result != FOUR_G_MQTT_OK)
    {
        return result;
    }

    result = FourG_SendRawBytes(payload, payloadLength);
    if (result != FOUR_G_MQTT_OK)
    {
        return result;
    }

    result = FourG_WaitForToken("OK", FOUR_G_PUBLISH_OK_TIMEOUT_MS);
    if (result == FOUR_G_MQTT_OK)
    {
        gLastPublishTick = HAL_GetTick();
    }
    else
    {
        gMqttConnected = false;
    }

    return result;
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
        if (HAL_UART_Receive(gFourGUart, &byte, 1U, 20U) == HAL_OK)
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

static FourGMqttResult_t FourG_WaitForToken(const char *token, uint32_t timeoutMs)
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
        if (HAL_UART_Receive(gFourGUart, &byte, 1U, 20U) == HAL_OK)
        {
            if (index < (sizeof(response) - 1U))
            {
                response[index++] = (char)byte;
                response[index] = '\0';
            }

            if (strstr(response, "ERROR") != NULL)
            {
                return FOUR_G_MQTT_ERROR;
            }

            if (strstr(response, token) != NULL)
            {
                return FOUR_G_MQTT_OK;
            }
        }
    }

    return FOUR_G_MQTT_TIMEOUT;
}

static FourGMqttResult_t FourG_WaitForPrompt(uint32_t timeoutMs)
{
    return FourG_WaitForToken(">", timeoutMs);
}

static void FourG_DrainInput(void)
{
    uint8_t byte;

    if (gFourGUart == NULL)
    {
        return;
    }

    while (HAL_UART_Receive(gFourGUart, &byte, 1U, 1U) == HAL_OK)
    {
    }
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

static bool FourG_ParseNetworkTime(FourGDateTime_t *dateTime)
{
    char response[FOUR_G_RX_BUFFER_SIZE];
    char *timeStart;
    FourGMqttResult_t result;
    int scanned;

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
