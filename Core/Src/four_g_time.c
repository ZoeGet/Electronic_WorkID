#include "four_g_time.h"
#include <stdio.h>
#include <string.h>

#define FOUR_G_TIME_COMMAND              "AT+CCLK?\r\n"
#define FOUR_G_TIME_RESPONSE_SIZE        128U
#define FOUR_G_TIME_UART_TX_TIMEOUT_MS   1000U
#define FOUR_G_TIME_MIN_VALID_YEAR       2024U

typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  int16_t timezone_quarters;
} FourGTimeRaw_t;

static UART_HandleTypeDef *gFourGTimeUart = NULL;
static char gFourGTimeAsyncResponse[FOUR_G_TIME_RESPONSE_SIZE];
static uint16_t gFourGTimeAsyncIndex = 0U;
static uint32_t gFourGTimeAsyncStartTick = 0U;
static uint32_t gFourGTimeAsyncTimeoutMs = 0U;
static bool gFourGTimeAsyncActive = false;

static bool FourG_Time_IsLeapYear(uint16_t year)
{
  return (((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U)));
}

static uint8_t FourG_Time_DaysInMonth(uint16_t year, uint8_t month)
{
  static const uint8_t days[12] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

  if ((month < 1U) || (month > 12U)) {
    return 0U;
  }

  if ((month == 2U) && FourG_Time_IsLeapYear(year)) {
    return 29U;
  }

  return days[month - 1U];
}

static bool FourG_Time_IsValid(const FourGTime_t *time)
{
  uint8_t days;

  if (time == NULL) {
    return false;
  }

  if ((time->year < FOUR_G_TIME_MIN_VALID_YEAR) || (time->month < 1U) || (time->month > 12U) ||
      (time->hour > 23U) || (time->minute > 59U) || (time->second > 59U)) {
    return false;
  }

  days = FourG_Time_DaysInMonth(time->year, time->month);
  return (days != 0U) && (time->day >= 1U) && (time->day <= days);
}

static bool FourG_Time_IsDefaultUnsyncedRawTime(const FourGTimeRaw_t *raw)
{
  if (raw == NULL) {
    return true;
  }

  return (raw->hour == 0U) && (raw->minute == 0U) && (raw->second == 0U);
}

static void FourG_Time_AddOneDay(FourGTime_t *time)
{
  uint8_t days;

  if (time == NULL) {
    return;
  }

  days = FourG_Time_DaysInMonth(time->year, time->month);
  if (days == 0U) {
    return;
  }

  time->day++;
  if (time->day <= days) {
    return;
  }

  time->day = 1U;
  time->month++;
  if (time->month <= 12U) {
    return;
  }

  time->month = 1U;
  time->year++;
}

static void FourG_Time_SubOneDay(FourGTime_t *time)
{
  if (time == NULL) {
    return;
  }

  if (time->day > 1U) {
    time->day--;
    return;
  }

  if (time->month > 1U) {
    time->month--;
  } else {
    time->month = 12U;
    if (time->year > 0U) {
      time->year--;
    }
  }

  time->day = FourG_Time_DaysInMonth(time->year, time->month);
}

static void FourG_Time_AddTimezone(FourGTime_t *time, int16_t timezone_quarters)
{
  int32_t seconds_of_day;
  int32_t offset_seconds;

  if (time == NULL) {
    return;
  }

  seconds_of_day = ((int32_t)time->hour * 3600L) + ((int32_t)time->minute * 60L) + (int32_t)time->second;
  offset_seconds = (int32_t)timezone_quarters * 15L * 60L;
  seconds_of_day += offset_seconds;

  while (seconds_of_day >= 86400L) {
    seconds_of_day -= 86400L;
    FourG_Time_AddOneDay(time);
  }

  while (seconds_of_day < 0L) {
    seconds_of_day += 86400L;
    FourG_Time_SubOneDay(time);
  }

  time->hour = (uint8_t)(seconds_of_day / 3600L);
  seconds_of_day %= 3600L;
  time->minute = (uint8_t)(seconds_of_day / 60L);
  time->second = (uint8_t)(seconds_of_day % 60L);
}

static FourGTimeResult_t FourG_Time_ReadResponse(char *response, uint16_t response_size, uint32_t timeout_ms)
{
  uint32_t start_tick;
  uint16_t index = 0U;
  uint8_t byte;

  if ((gFourGTimeUart == NULL) || (response == NULL) || (response_size == 0U)) {
    return FOUR_G_TIME_INVALID_PARAM;
  }

  memset(response, 0, response_size);
  start_tick = HAL_GetTick();

  while ((HAL_GetTick() - start_tick) < timeout_ms) {
    if (HAL_UART_Receive(gFourGTimeUart, &byte, 1U, 20U) == HAL_OK) {
      if (index < (response_size - 1U)) {
        response[index++] = (char)byte;
        response[index] = '\0';
      }

      if ((strstr(response, "\r\nOK\r\n") != NULL) || (strstr(response, "ERROR") != NULL)) {
        break;
      }
    }
  }

  if (index == 0U) {
    return FOUR_G_TIME_TIMEOUT;
  }

  if (strstr(response, "ERROR") != NULL) {
    return FOUR_G_TIME_UART_ERROR;
  }

  return (strstr(response, "+CCLK:") != NULL) ? FOUR_G_TIME_OK : FOUR_G_TIME_PARSE_ERROR;
}

static bool FourG_Time_ParseTwoDigits(const char *text, uint8_t *value)
{
  if ((text == NULL) || (value == NULL) ||
      (text[0] < '0') || (text[0] > '9') ||
      (text[1] < '0') || (text[1] > '9')) {
    return false;
  }

  *value = (uint8_t)(((uint8_t)(text[0] - '0') * 10U) + (uint8_t)(text[1] - '0'));
  return true;
}

static bool FourG_Time_ParseTimezone(const char *text, const char *end, int16_t *timezone_quarters)
{
  int16_t value = 0;
  int16_t sign = 1;
  const char *cursor;

  if ((text == NULL) || (end == NULL) || (timezone_quarters == NULL) || (text >= end)) {
    return false;
  }

  if (*text == '+') {
    sign = 1;
  } else if (*text == '-') {
    sign = -1;
  } else {
    return false;
  }

  cursor = text + 1;
  if (cursor >= end) {
    return false;
  }

  while (cursor < end) {
    if ((*cursor < '0') || (*cursor > '9')) {
      return false;
    }
    value = (int16_t)((value * 10) + (*cursor - '0'));
    cursor++;
  }

  *timezone_quarters = (int16_t)(sign * value);
  return true;
}

static FourGTimeResult_t FourG_Time_ParseResponse(const char *response, FourGTime_t *time)
{
  FourGTimeRaw_t raw = {0};
  const char *cclk;
  const char *quote_start;
  const char *quote_end;

  if ((response == NULL) || (time == NULL)) {
    return FOUR_G_TIME_INVALID_PARAM;
  }

  cclk = strstr(response, "+CCLK:");
  if (cclk == NULL) {
    return FOUR_G_TIME_PARSE_ERROR;
  }

  quote_start = strchr(cclk, '"');
  if (quote_start == NULL) {
    return FOUR_G_TIME_PARSE_ERROR;
  }
  quote_start++;

  quote_end = strchr(quote_start, '"');
  if ((quote_end == NULL) || ((quote_end - quote_start) < 18)) {
    return FOUR_G_TIME_PARSE_ERROR;
  }

  if ((quote_start[2] != '/') || (quote_start[5] != '/') ||
      (quote_start[8] != ',') || (quote_start[11] != ':') ||
      (quote_start[14] != ':')) {
    return FOUR_G_TIME_PARSE_ERROR;
  }

  {
    uint8_t year2 = 0U;
    if (!FourG_Time_ParseTwoDigits(&quote_start[0], &year2)) {
      return FOUR_G_TIME_PARSE_ERROR;
    }
    raw.year = (uint16_t)(2000U + year2);
  }

  if (!FourG_Time_ParseTwoDigits(&quote_start[3], &raw.month) ||
      !FourG_Time_ParseTwoDigits(&quote_start[6], &raw.day) ||
      !FourG_Time_ParseTwoDigits(&quote_start[9], &raw.hour) ||
      !FourG_Time_ParseTwoDigits(&quote_start[12], &raw.minute) ||
      !FourG_Time_ParseTwoDigits(&quote_start[15], &raw.second) ||
      !FourG_Time_ParseTimezone(&quote_start[17], quote_end, &raw.timezone_quarters)) {
    return FOUR_G_TIME_PARSE_ERROR;
  }

  if (FourG_Time_IsDefaultUnsyncedRawTime(&raw)) {
    return FOUR_G_TIME_INVALID_TIME;
  }

  time->year = raw.year;
  time->month = raw.month;
  time->day = raw.day;
  time->hour = raw.hour;
  time->minute = raw.minute;
  time->second = raw.second;

  if (!FourG_Time_IsValid(time)) {
    return FOUR_G_TIME_INVALID_TIME;
  }

  FourG_Time_AddTimezone(time, raw.timezone_quarters);

  return FourG_Time_IsValid(time) ? FOUR_G_TIME_OK : FOUR_G_TIME_INVALID_TIME;
}

void FourG_Time_Init(UART_HandleTypeDef *huart)
{
  gFourGTimeUart = huart;
}

const char *FourG_Time_ResultText(FourGTimeResult_t result)
{
  switch (result) {
    case FOUR_G_TIME_OK:
      return "OK";
    case FOUR_G_TIME_INVALID_PARAM:
      return "PARAM";
    case FOUR_G_TIME_UART_ERROR:
      return "UART";
    case FOUR_G_TIME_TIMEOUT:
      return "TMO";
    case FOUR_G_TIME_PARSE_ERROR:
      return "PARSE";
    case FOUR_G_TIME_INVALID_TIME:
      return "INV";
    default:
      return "UNK";
  }
}

void FourG_Time_AsyncCancel(void)
{
  gFourGTimeAsyncActive = false;
  gFourGTimeAsyncIndex = 0U;
  memset(gFourGTimeAsyncResponse, 0, sizeof(gFourGTimeAsyncResponse));
}

FourGTimeResult_t FourG_Time_AsyncStart(uint32_t timeout_ms)
{
  if (gFourGTimeUart == NULL) {
    return FOUR_G_TIME_INVALID_PARAM;
  }

  FourG_Time_AsyncCancel();
  (void)HAL_UART_AbortReceive(gFourGTimeUart);
  (void)HAL_UART_AbortTransmit(gFourGTimeUart);
  __HAL_UART_CLEAR_OREFLAG(gFourGTimeUart);
  __HAL_UART_CLEAR_FEFLAG(gFourGTimeUart);
  __HAL_UART_CLEAR_NEFLAG(gFourGTimeUart);
  __HAL_UART_CLEAR_PEFLAG(gFourGTimeUart);

  if (HAL_UART_Transmit(gFourGTimeUart, (uint8_t *)FOUR_G_TIME_COMMAND,
                        (uint16_t)strlen(FOUR_G_TIME_COMMAND), FOUR_G_TIME_UART_TX_TIMEOUT_MS) != HAL_OK) {
    return FOUR_G_TIME_UART_ERROR;
  }

  gFourGTimeAsyncStartTick = HAL_GetTick();
  gFourGTimeAsyncTimeoutMs = timeout_ms;
  gFourGTimeAsyncActive = true;
  return FOUR_G_TIME_OK;
}

FourGTimeAsyncStatus_t FourG_Time_AsyncProcess(FourGTime_t *time, FourGTimeResult_t *result)
{
  uint8_t byte;

  if (result != NULL) {
    *result = FOUR_G_TIME_OK;
  }

  if ((gFourGTimeUart == NULL) || (time == NULL) || (result == NULL)) {
    if (result != NULL) {
      *result = FOUR_G_TIME_INVALID_PARAM;
    }
    return FOUR_G_TIME_ASYNC_ERROR;
  }

  if (!gFourGTimeAsyncActive) {
    return FOUR_G_TIME_ASYNC_IDLE;
  }

  while (HAL_UART_Receive(gFourGTimeUart, &byte, 1U, 0U) == HAL_OK) {
    if (gFourGTimeAsyncIndex < (sizeof(gFourGTimeAsyncResponse) - 1U)) {
      gFourGTimeAsyncResponse[gFourGTimeAsyncIndex++] = (char)byte;
      gFourGTimeAsyncResponse[gFourGTimeAsyncIndex] = '\0';
    }

    if (strstr(gFourGTimeAsyncResponse, "ERROR") != NULL) {
      gFourGTimeAsyncActive = false;
      *result = FOUR_G_TIME_UART_ERROR;
      return FOUR_G_TIME_ASYNC_ERROR;
    }

    if (strstr(gFourGTimeAsyncResponse, "\r\nOK\r\n") != NULL) {
      gFourGTimeAsyncActive = false;
      if (strstr(gFourGTimeAsyncResponse, "+CCLK:") == NULL) {
        *result = FOUR_G_TIME_PARSE_ERROR;
        return FOUR_G_TIME_ASYNC_ERROR;
      }
      *result = FourG_Time_ParseResponse(gFourGTimeAsyncResponse, time);
      return (*result == FOUR_G_TIME_OK) ? FOUR_G_TIME_ASYNC_DONE : FOUR_G_TIME_ASYNC_ERROR;
    }
  }

  if ((HAL_GetTick() - gFourGTimeAsyncStartTick) >= gFourGTimeAsyncTimeoutMs) {
    gFourGTimeAsyncActive = false;
    *result = (gFourGTimeAsyncIndex == 0U) ? FOUR_G_TIME_TIMEOUT : FOUR_G_TIME_PARSE_ERROR;
    return FOUR_G_TIME_ASYNC_ERROR;
  }

  return FOUR_G_TIME_ASYNC_BUSY;
}

FourGTimeResult_t FourG_Time_Get(FourGTime_t *time, uint32_t timeout_ms)
{
  char response[FOUR_G_TIME_RESPONSE_SIZE];
  FourGTimeResult_t result;

  if ((gFourGTimeUart == NULL) || (time == NULL)) {
    return FOUR_G_TIME_INVALID_PARAM;
  }

  (void)HAL_UART_AbortReceive(gFourGTimeUart);
  (void)HAL_UART_AbortTransmit(gFourGTimeUart);
  __HAL_UART_CLEAR_OREFLAG(gFourGTimeUart);
  __HAL_UART_CLEAR_FEFLAG(gFourGTimeUart);
  __HAL_UART_CLEAR_NEFLAG(gFourGTimeUart);
  __HAL_UART_CLEAR_PEFLAG(gFourGTimeUart);

  if (HAL_UART_Transmit(gFourGTimeUart, (uint8_t *)FOUR_G_TIME_COMMAND,
                        (uint16_t)strlen(FOUR_G_TIME_COMMAND), FOUR_G_TIME_UART_TX_TIMEOUT_MS) != HAL_OK) {
    return FOUR_G_TIME_UART_ERROR;
  }

  result = FourG_Time_ReadResponse(response, sizeof(response), timeout_ms);
  if (result != FOUR_G_TIME_OK) {
    return result;
  }

  return FourG_Time_ParseResponse(response, time);
}
