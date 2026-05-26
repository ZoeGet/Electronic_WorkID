# MAX9814 录音系统集成指南

## 重要提示

**本文档用于指导如何在现有项目中集成 MAX9814 录音功能**

## 一、CubeMX 配置步骤

### 1.1 打开现有项目

1. 打开 STM32CubeMX
2. 打开现有项目文件：`Electronic_WorkID.ioc`

### 1.2 配置 ADC1

**Pinout 配置：**
- 点击芯片左侧的 **PA1** 引脚
- 选择 **ADC1_IN1**

**Parameter Settings：**
```
Clock Prescaler: Synchronous clock mode (PCLK2/4)
Resolution: 12 bits
Data Alignment: Right Alignment
Scan Conversion Mode: Disable
Continuous Conversion Mode: Enable
Low Power Auto Wait: DISABLE
Low Power Auto Power Off: DISABLE
DMA Conversion Mode: Circular
External Trigger Conversion Source: TIM2 TRGO
External Trigger Edge: Rising Edge
DMA Request: Enable
Oversampling Mode: Disable
```

**DMA Settings：**
```
DMA Request: ADC1
DMA Stream: DMA2 Stream1
Direction: Peripheral to Memory
Mode: Circular
Data Width: Word (32-bit)
Priority: Very High
```

**NVIC Settings：**
```
DMA2 Stream1 global interrupt: Enable
Preemption Priority: 0
Sub Priority: 0
```

**ADC1 Channel 1 (PA1)：**
```
Sampling Time: 480 Cycles
```

### 1.3 配置 TIM2

**Parameter Settings：**
```
Prescaler: 0
Counter Mode: Up
Counter Period: 4999
Auto-Reload Preload: Enable
```

**Clock Source：**
```
Clock Source: Internal Clock
```

**Trigger Output (TRGO)：**
```
Master Mode Output Source: Update
```

### 1.4 配置 SDMMC1

**Pinout 配置：**
- 确保已配置：PD2 (CMD), PC12 (CK), PC8 (D0)

**Parameter Settings：**
```
Mode: 1-bit mode
Hardware Flow Control: Enable
Clock Edge: Rising Edge
Clock Power Save: Disable
Bus Speed: 25 MHz
```

**DMA Settings：**
```
DMA Request: SDMMC1
DMA Stream TX: DMA2 Stream4
DMA Stream RX: DMA2 Stream5
Direction (TX): Memory to Peripheral
Direction (RX): Peripheral to Memory
Mode: Normal
Data Width: Word (32-bit)
Priority: High
```

**NVIC Settings：**
```
SDMMC1 global interrupt: Enable
Preemption Priority: 1
Sub Priority: 0
```

### 1.5 配置 FATFS

**Middleware 配置：**
- 确保已启用 FATFS

**Parameter Settings：**
```
Enable FATFS: Yes
Logical Drive Number: 0
FAT File System Type: FAT16
Use Default BSP: No
Code Conversion Page: OEM (850)
```

**SDIO Interface Configuration：**
```
Use polling: No
Use DMA: Yes
Use Cache: Yes
```

### 1.6 GPIO 配置

**PA1 (ADC1_IN1)：**
```
Mode: Analog mode
```

**SDMMC 引脚：**
```
PD2 (SDMMC1_CMD): Alternate Function Push Pull
PC12 (SDMMC1_CK): Alternate Function Push Pull
PC8 (SDMMC1_D0): Alternate Function Push Pull
Pull-up/Pull-down: Pull-up
Speed: Very High
```

### 1.7 生成代码

- 点击 **Project Manager** → **Generate Code**
- 确认所有配置无误

## 二、代码集成步骤

### 2.1 复制源文件

将以下文件复制到项目中：

**头文件 (Core/Inc/)：**
- `wav_format.h`
- `buffer_manager.h`
- `recorder.h`

**源文件 (Core/Src/)：**
- `wav_format.c`
- `buffer_manager.c`
- `recorder.c`

### 2.2 修改 main.c

**添加包含文件：**
```c
#include "recorder.h"
#include "buffer_manager.h"
#include "wav_format.h"
```

**添加全局变量：**
```c
/* 录音系统句柄 */
static Recorder_Handle_t recorder;
```

**在 main() 函数中添加初始化代码：**
```c
int main(void) {
    /* MCU 初始化 */
    HAL_Init();
    SystemClock_Config();
    
    /* 外设初始化 */
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_TIM2_Init();
    MX_SDMMC1_SD_Init();
    MX_FATFS_Init();
    
    /* 录音系统初始化 */
    if (Recorder_Init(&recorder) != RECORDER_OK) {
        Error_Handler();
    }
    
    /* 挂载 SD 卡 */
    if (f_mount(&SDFatFs, (TCHAR const*)"", 1) != FR_OK) {
        Error_Handler();
    }
    
    /* 主循环 */
    while (1) {
        /* 使用示例 */
    }
}
```

**添加回调函数：**
```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        Recorder_ADC_Callback(&recorder);
    }
}

void HAL_SD_WriteCpltCallback(SD_HandleTypeDef* hsd) {
    if (hsd->Instance == SDMMC1) {
        Recorder_SD_Callback(&recorder);
    }
}
```

### 2.3 编译项目

1. 清理项目
2. 重新编译项目
3. 解决可能出现的编译错误

## 三、使用示例

### 3.1 基本录音

```c
/* 开始录音 */
if (Recorder_Start(&recorder) == RECORDER_OK) {
    /* 录音 10 秒 */
    HAL_Delay(10000);
    
    /* 停止录音 */
    Recorder_Stop(&recorder);
}
```

### 3.2 暂停和恢复

```c
/* 暂停录音 */
Recorder_Pause(&recorder);

/* 做一些其他事情 */
HAL_Delay(5000);

/* 恢复录音 */
Recorder_Resume(&recorder);
```

### 3.3 获取录音状态

```c
/* 获取录音时长 */
uint32_t duration = Recorder_GetDuration(&recorder);

/* 获取文件大小 */
uint32_t size = Recorder_GetFileSize(&recorder);

/* 获取状态 */
Recorder_State_t state = Recorder_GetState(&recorder);
```

## 四、调试指南

### 4.1 验证 ADC 采样

使用示波器或逻辑分析仪验证：
- PA1 引脚是否有音频信号输入
- ADC 转换是否正常
- DMA 传输是否连续

### 4.2 验证 TIM2 触发

验证 TIM2 配置：
```c
/* 检查 TIM2 配置 */
printf("TIM2 Prescaler: %lu\r\n", htim2.Init.Prescaler);
printf("TIM2 Period: %lu\r\n", htim2.Init.Period);
/* 应该输出：0 和 4999 */
```

### 4.3 验证 SD 卡写入

验证 SD 卡操作：
```c
/* 测试 SD 卡读写 */
FATFS fs;
FIL file;
if (f_mount(&fs, "", 1) == FR_OK) {
    printf("SD 卡挂载成功\r\n");
    
    if (f_open(&file, "test.txt", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
        UINT written;
        f_write(&file, "Hello", 5, &written);
        f_close(&file);
        printf("文件写入成功\r\n");
    }
}
```

### 4.4 验证 WAV 文件

录音完成后：
1. 拔出 SD 卡
2. 在 PC 上打开 WAV 文件
3. 验证是否能正常播放
4. 检查音质是否清晰

## 五、常见问题

### Q1: 采样率不准确

**解决：**
- 检查 TIM2 配置：Prescaler=0, Period=4999
- 确认系统时钟为 80MHz
- 验证 TIM2 TRGO 是否连接到 ADC1

### Q2: 录音有杂音

**解决：**
- 检查电源滤波
- 优化 PCB 布局
- 检查 MAX9814 增益设置
- 验证 ADC 参考电压稳定性

### Q3: SD 卡写入失败

**解决：**
- 检查 SD 卡格式（FAT16）
- 验证 SD 卡容量（≤2GB）
- 检查 SDMMC 时钟频率
- 启用重试机制

### Q4: 缓冲区溢出

**解决：**
- 增加缓冲区数量
- 提高 SD 卡写入速度
- 降低采样率
- 优化 FATFS 配置

## 六、性能优化

### 6.1 DMA 优化

- 使用 DMA 循环模式
- 设置合适的优先级
- 减少 CPU 干预

### 6.2 FATFS 优化

- 启用写入缓冲
- 批量写入（512 字节对齐）
- 预分配文件空间

### 6.3 内存优化

- 精简缓冲区大小
- 优化其他模块内存使用
- 必要时升级芯片

## 七、下一步

完成基本功能后，可以考虑：

1. 添加录音文件管理功能
2. 实现循环录音
3. 添加电量检测和低电量保护
4. 实现录音文件加密
5. 添加网络传输功能

---

**创建日期**: 2026-04-22  
**版本**: 1.0  
**适用芯片**: STM32L431RCT6
