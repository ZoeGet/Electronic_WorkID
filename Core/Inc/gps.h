#ifndef __GPS_H__
#define __GPS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"
#include "string.h"
#include "stdio.h"

// 类型定义（避免依赖 main.h）
typedef uint32_t  u32;
typedef uint16_t u16;
typedef uint8_t  u8;

/* Exported defines ----------------------------------------------------------*/
#define USART_REC_LEN        200    // 定义最大接收字节数 200
#define EN_USART2_RX         1      // 使能（1）/禁止（0）串口 2 接收
#define false                0
#define true                 1

// 定义数组长度
#define GPS_Buffer_Length    128
#define UTCTime_Length       11
#define latitude_Length      11
#define N_S_Length           2
#define longitude_Length     12
#define E_W_Length           2

/* Exported types ------------------------------------------------------------*/
// 存放接收数据的结构体
typedef struct {
    char GPS_Buffer[GPS_Buffer_Length];    // 原始数据缓冲区
    volatile char isGetData;               // 是否获取到 GPS 数据
    volatile char isParseData;             // 是否解析完成
    char UTCTime[UTCTime_Length];          // UTC 时间
    char latitude[latitude_Length];        // 纬度
    char N_S[N_S_Length];                  // N/S
    char longitude[longitude_Length];      // 经度
    char E_W[E_W_Length];                  // E/W
    volatile char isUsefull;               // 定位信息是否有效
} GPS_SaveData;

/* Exported variables --------------------------------------------------------*/
extern char rxdatabufer;
extern u16 point1;
extern GPS_SaveData Save_Data;
extern u8 USART_RX_BUF[USART_REC_LEN];    // 接收缓冲，最大 USART_REC_LEN 个字节

/* Exported functions prototypes ---------------------------------------------*/
void GPS_Init(void);                               // GPS 模块初始化
void GPS_USART_Process(void);                      // GPS 串口数据处理（在中断中调用）
void CLR_Buf(void);                                  // 清空缓冲区
u8 Hand(char *a);                                    // 串口命令识别
void clrStruct(void);                                // 清空结构体
void errorLog(int num);                              // 错误日志
void parseGpsBuffer(void);                           // 解析 GPS 数据
void printGpsBuffer(void);                           // 打印 GPS 数据（调试用）
float ConvertLatitude(char *latitude, char direction);    // 纬度转换
float ConvertLongitude(char *longitude, char direction);  // 经度转换
uint32_t GPS_GetRxCount(void);                       // 获取接收计数（调试用）
void GPS_ResetRxCount(void);                         // 重置接收计数

#ifdef __cplusplus
}
#endif

#endif /* __GPS_H__ */
