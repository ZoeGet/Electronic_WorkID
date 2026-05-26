#include "gps.h"
#include "usart.h"
#include "stdlib.h"
#include "string.h"


char rxdatabufer;
u16 point1 = 0;
GPS_SaveData Save_Data;
u8 USART_RX_BUF[USART_REC_LEN];    // 接收缓冲，最大 200 个字节
static uint8_t rxChar = 0;          // 单字节接收缓冲（用于DMA或中断模式）
static uint32_t rxByteCount = 0;   // 接收字节计数（调试用）

/* Private function prototypes -----------------------------------------------*/
static float ConvertCoordinate(char *coordinate, char direction);
/* Private user code ---------------------------------------------------------*/

/**
  * @brief  坐标转换函数（度分格式 → 十进制度）
  *         将 ddmm.mmmm 格式转换为 dd.dddddd 格式
  * @param  coordinate: 原始坐标字符串（如 "2236.9453"）
  * @param  direction: 方向字符（N/S/E/W）
  * @retval 转换后的十进制度坐标
  */
static float ConvertCoordinate(char *coordinate, char direction)
{
    float decimalDegree = 0.0f;
    float degrees = 0.0f;
    float minutes = 0.0f;
    char degreeStr[4] = {0};
    int i = 0;
    size_t coordinateLen = 0U;

    if (coordinate == NULL)
    {
        return 0.0f;
    }

    coordinateLen = strlen(coordinate);
    if (((direction == 'N') || (direction == 'S')) && (coordinateLen < 4U))
    {
        return 0.0f;
    }

    if (((direction == 'E') || (direction == 'W')) && (coordinateLen < 5U))
    {
        return 0.0f;
    }

    if ((direction != 'N') && (direction != 'S') && (direction != 'E') && (direction != 'W'))
    {
        return 0.0f;
    }
    
    // 提取度数部分
    // 纬度：前 2 位，经度：前 3 位
    if (direction == 'N' || direction == 'S')
    {
        // 纬度：ddmm.mmmm，提取前 2 位
        degreeStr[0] = coordinate[0];
        degreeStr[1] = coordinate[1];
        i = 2;
    }
    else
    {
        // 经度：dddmm.mmmm，提取前 3 位
        degreeStr[0] = coordinate[0];
        degreeStr[1] = coordinate[1];
        degreeStr[2] = coordinate[2];
        i = 3;
    }
    
    // 将度数字符串转换为浮点数
    degrees = (float)atoi(degreeStr);
    
    // 提取分钟部分并转换为浮点数
    minutes = (float)atof(&coordinate[i]);
    
    // 计算十进制度：度 + 分/60
    decimalDegree = degrees + (minutes / 60.0f);
    
    // 处理方向：南纬和西经为负值
    if (direction == 'S' || direction == 'W')
    {
        decimalDegree = -decimalDegree;
    }
    
    return decimalDegree;
}

/**
  * @brief  纬度转换函数
  *         将 ddmm.mmmm 格式转换为 dd.dddddd 格式
  * @param  latitude: 原始纬度字符串
  * @param  direction: 方向（N/S）
  * @retval 转换后的十进制度纬度
  */
float ConvertLatitude(char *latitude, char direction)
{
    return ConvertCoordinate(latitude, direction);
}

/**
  * @brief  经度转换函数
  *         将 dddmm.mmmm 格式转换为 ddd.dddddd 格式
  * @param  longitude: 原始经度字符串
  * @param  direction: 方向（E/W）
  * @retval 转换后的十进制度经度
  */
float ConvertLongitude(char *longitude, char direction)
{
    return ConvertCoordinate(longitude, direction);
}

#if EN_USART2_RX

/**
  * @brief  GPS 模块初始化
  *         使用 CubeMX 生成的 MX_USART2_UART_Init()
  *         启动中断接收模式
  * @retval None
  */
void GPS_Init(void)
{
    // MX_USART2_UART_Init() 已由 CubeMX 生成并自动调用
    // 清空缓冲区
    CLR_Buf();
    // 启动中断接收第一个字节
    HAL_UART_Receive_IT(&huart2, &rxChar, 1);
}

/**
  * @brief  UART接收完成回调函数（HAL库自动调用）
  *         每接收到1个字节后自动触发，继续接收下一个字节
  * @param  huart: UART句柄
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        rxByteCount++;
        GPS_USART_Process();
        HAL_UART_Receive_IT(&huart2, &rxChar, 1);
    }
}

/**
  * @brief  GPS 数据接收处理函数（由HAL_UART_RxCpltCallback调用）
  *         处理从GPS模块接收的NMEA数据
  * @retval None
  */
void GPS_USART_Process(void)
{
    u8 Res = rxChar;  // 使用回调传入的数据

    if (Res == '$')
    {
        point1 = 0;
        memset(USART_RX_BUF, 0, USART_REC_LEN);
    }

    if (point1 < (USART_REC_LEN - 1U))
    {
        USART_RX_BUF[point1++] = Res;
        USART_RX_BUF[point1] = '\0';
    }
    else
    {
        CLR_Buf();
        return;
    }

    if (point1 >= 6U)
    {
        if (USART_RX_BUF[0] == '$' &&
            USART_RX_BUF[1] == 'G' &&
            USART_RX_BUF[2] == 'N' &&
            USART_RX_BUF[3] == 'R' &&
            USART_RX_BUF[4] == 'M' &&
            USART_RX_BUF[5] == 'C')
        {
            if ((Res == '\n') || (Res == '\r'))
            {
                uint16_t copyLen = point1;
                if (copyLen >= GPS_Buffer_Length)
                {
                    copyLen = GPS_Buffer_Length - 1U;
                }

                memset(Save_Data.GPS_Buffer, 0, GPS_Buffer_Length);
                memcpy(Save_Data.GPS_Buffer, USART_RX_BUF, copyLen);
                Save_Data.GPS_Buffer[copyLen] = '\0';
                Save_Data.isGetData = true;
                CLR_Buf();
            }
        }
        else if ((Res == '\n') || (Res == '\r'))
        {
            CLR_Buf();
        }
    }
}

/**
  * @brief  清空接收缓冲区
  * @retval None
  */
void CLR_Buf(void)
{
    memset(USART_RX_BUF, 0, USART_REC_LEN);
    point1 = 0;
}

/**
  * @brief  串口命令识别（可选功能）
  * @param  a: 要查找的字符串
  * @retval 1=找到，0=未找到
  */
u8 Hand(char *a)
{
    if (strstr((char*)USART_RX_BUF, a) != NULL)
        return 1;
    else
        return 0;
}

/**
  * @brief  清空 GPS 数据结构
  * @retval None
  */
void clrStruct(void)
{
    Save_Data.isGetData = false;
    Save_Data.isParseData = false;
    Save_Data.isUsefull = false;
    memset(Save_Data.GPS_Buffer, 0, GPS_Buffer_Length);
    memset(Save_Data.UTCTime, 0, UTCTime_Length);
    memset(Save_Data.latitude, 0, latitude_Length);
    memset(Save_Data.N_S, 0, N_S_Length);
    memset(Save_Data.longitude, 0, longitude_Length);
    memset(Save_Data.E_W, 0, E_W_Length);
}

/**
  * @brief  错误日志（解析失败时调用）
  * @param  num: 错误编号
  * @retval None
  */
void errorLog(int num)
{
    // 暂时不处理，只清空数据，继续接收
    // 实际应用中可以通过 LED 或串口输出错误信息
    clrStruct();
}

/**
  * @brief  解析 GPS 缓冲区中的 GNRMC 语句
  *         提取 UTC 时间、纬度、经度等信息
  * @retval None
  */
void parseGpsBuffer(void)
{
    char gpsBuffer[GPS_Buffer_Length];
    char *field;
    uint8_t index = 0U;
    char status = 'V';

    if (!Save_Data.isGetData)
    {
        return;
    }

    Save_Data.isGetData = false;
    Save_Data.isParseData = false;
    Save_Data.isUsefull = false;
    memset(Save_Data.UTCTime, 0, UTCTime_Length);
    memset(Save_Data.latitude, 0, latitude_Length);
    memset(Save_Data.N_S, 0, N_S_Length);
    memset(Save_Data.longitude, 0, longitude_Length);
    memset(Save_Data.E_W, 0, E_W_Length);

    memcpy(gpsBuffer, Save_Data.GPS_Buffer, GPS_Buffer_Length);
    gpsBuffer[GPS_Buffer_Length - 1U] = '\0';

    field = strtok(gpsBuffer, ",");
    while (field != NULL)
    {
        switch (index)
        {
            case 0U:
                if (strcmp(field, "$GNRMC") != 0)
                {
                    errorLog(1);
                    return;
                }
                break;

            case 1U:
                strncpy(Save_Data.UTCTime, field, UTCTime_Length - 1U);
                break;

            case 2U:
                status = field[0];
                break;

            case 3U:
                strncpy(Save_Data.latitude, field, latitude_Length - 1U);
                break;

            case 4U:
                Save_Data.N_S[0] = field[0];
                Save_Data.N_S[1] = '\0';
                break;

            case 5U:
                strncpy(Save_Data.longitude, field, longitude_Length - 1U);
                break;

            case 6U:
                Save_Data.E_W[0] = field[0];
                Save_Data.E_W[1] = '\0';
                Save_Data.isUsefull = (status == 'A') ? true : false;
                Save_Data.isParseData = true;
                return;

            default:
                break;
        }

        index++;
        field = strtok(NULL, ",");
    }

    errorLog(2);
}

/**
  * @brief  打印 GPS 数据到串口（调试用）
  * @retval None
  */
void printGpsBuffer(void)
{
    if (Save_Data.isParseData)
    {
        Save_Data.isParseData = false;

        printf("UTCTime: %s\r\n", Save_Data.UTCTime);

        if (Save_Data.isUsefull)
        {
            printf("Latitude: %s %s\r\n", Save_Data.latitude, Save_Data.N_S);
            printf("Longitude: %s %s\r\n", Save_Data.longitude, Save_Data.E_W);
        }
        else
        {
            printf("GPS DATA is not usefull!\r\n");
        }
    }
}

uint32_t GPS_GetRxCount(void)
{
    return rxByteCount;
}

void GPS_ResetRxCount(void)
{
    rxByteCount = 0;
}

#endif /* EN_USART2_RX */
