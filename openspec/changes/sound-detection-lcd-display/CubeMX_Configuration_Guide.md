# STM32CubeMX 配置指南 - 声音检测功能

本文档详细说明如何在 STM32CubeMX 中配置声音检测功能所需的硬件外设。

---

## 📋 配置总览

| 外设 | 配置项 | 参数值 | 说明 |
|------|--------|--------|------|
| **ADC1** | Channel 1 | PC0 | MAX9814 音频输入 |
| **TIM2** | Prescaler | 0 | 定时器预分频 |
| **TIM2** | Counter Period | 4999 | 自动重装载值 |
| **TIM2** | TRGO Output | Update Event | ADC 触发源 |
| **DMA** | Mode | Circular | 循环传输模式 |
| **DMA** | Data Width | Half Word | 16 位数据宽度 |

---

## 🔧 详细配置步骤

### 1. ADC1 配置（模拟 - 数字转换器）

#### 1.1 Pinout 配置
```
Path: Pinout & Configuration → Analog → ADC1
```

**操作：**
1. 勾选 **Channel 1 (IN1)**
2. 确认引脚映射为 **PC0**
3. 检查 PC0 引脚在芯片视图上显示为黄色（ADC 功能）

**验证要点：**
- ✅ PC0 必须配置为 **ADC1_IN1** 功能
- ✅ 不能配置为 GPIO 或其他复用功能
- ✅ 引脚名称显示为 **PC0 (ADC1_IN1)**

---

#### 1.2 Parameter Settings 配置
```
Path: Configuration → Analog → ADC1 → Parameter Settings
```

**关键参数设置：**

| 参数 | 值 | 说明 |
|------|-----|------|
| Resolution | 12 Bits | 12 位分辨率 |
| Data Alignment | Right Alignment | 右对齐 |
| Scan Conversion Mode | Disable | 单通道，禁用扫描 |
| Continuous Conversion Mode | **Disable** | ⚠️ 必须禁用（使用外部触发） |
| Discontinuous Conversion Mode | Disable | 禁用不连续模式 |
| **External Trigger Conversion Source** | **Timer 2 Trigger Out** | ⚠️ 关键：TIM2 触发 |
| **External Trigger Conversion Edge** | **Trigger Rising Edge** | 上升沿触发 |
| **DMA Continuous Requests** | **Enable** | ⚠️ 启用 DMA 连续请求 |
| Overrun Behavior | Overrun Behavior Disabled | 禁用覆盖 |

**配置截图位置：**
- 找到 "External Trigger Conversion Source" 下拉框
- 选择 **Timer 2 Trigger Out** 选项
- 确认 "DMA Continuous Requests" 已勾选

---

### 2. TIM2 配置（定时器触发源）

#### 2.1 Parameter Settings 配置
```
Path: Configuration → Timers → TIM2 → Parameter Settings
```

**Counter Settings：**

| 参数 | 值 | 计算说明 |
|------|-----|----------|
| **Prescaler (PSC)** | **0** | 不分频，80MHz 直接输入 |
| **Counter Period (ARR)** | **4999** | 80M / (0+1) / 5000 = 16kHz |
| Counter Mode | Up | 向上计数 |
| Auto-reload Preload | Enable | 启用自动重装载预装载 |

**验证计算公式：**
```
采样率 = 定时器时钟 / (ARR + 1)
16000 = 80,000,000 / (4999 + 1)
16000 = 80,000,000 / 5000 ✓
```

**⚠️ 常见错误：**
- ❌ PSC 设置为 4 → 采样率变成 3.2kHz（错误）
- ❌ ARR 设置为 999 → 采样率变成 80kHz（错误）
- ❌ Counter Mode 设置为 Down → 可能触发异常

---

#### 2.2 Trigger Output (TRGO) 配置
```
Path: Configuration → Timers → TIM2 → Parameter Settings → Trigger Output
```

**关键设置：**

| 参数 | 值 | 说明 |
|------|-----|------|
| Master/Slave Mode | Disable | 禁用主从模式 |
| **Trigger Event Selection (TRGO)** | **Update Event** | ⚠️ 必须选择更新事件 |

**为什么选择 Update Event？**
- TRGO 输出定时器的更新事件（计数器溢出）
- ADC 在每次定时器更新时触发采样
- 保证精确的 16kHz 采样率

**验证方法：**
- 展开 "Trigger Output (TRGO) Parameters"
- 确认 "Trigger Event Selection TRGO" 显示为 **Update Event**

---

### 3. DMA 配置（直接内存访问）

#### 3.1 DMA 设置
```
Path: Configuration → System Core → DMA
```

**添加 DMA 请求：**
1. 点击 **Add** 按钮
2. 选择 **ADC1** 作为 DMA 请求源
3. 确认 DMA 通道自动分配（通常是 DMA1_Channel1）

**DMA 参数配置：**

| 参数 | 值 | 说明 |
|------|-----|------|
| Direction | Peripheral to Memory | 外设到内存 |
| **Mode** | **Circular** | ⚠️ 循环模式（关键） |
| **Data Width (Peripheral)** | **Half Word** | 16 位（与 ADC 分辨率匹配） |
| **Data Width (Memory)** | **Half Word** | 16 位 |
| Priority | High | 高优先级 |

**⚠️ Mode 必须为 Circular：**
- 循环模式：DMA 传输完成后自动重新开始
- 确保连续采样无需 CPU 干预
- 如果选择 Normal，采样一次后停止

**验证要点：**
- ✅ 看到 DMA 请求列表中显示 **ADC1**
- ✅ Mode 列显示 **Circular**
- ✅ Data Width 显示 **Half Word**

---

#### 3.2 NVIC 配置（中断使能）
```
Path: Configuration → System Core → NVIC
```

**DMA 中断使能：**

| 中断 | 状态 | 说明 |
|------|------|------|
| DMA1 channel1 global interrupt | **Enabled** | 启用 DMA 中断 |

**为什么需要中断？**
- 半传输中断：处理前 64 个采样点
- 全传输中断：处理后 64 个采样点
- 在中断中调用 `AUDIO_Process()` 函数

**验证：**
- 找到 DMA1_Channel1 中断
- 确认复选框已勾选

---

### 4. 时钟配置（Clock Configuration）

#### 4.1 系统时钟
```
Path: Clock Configuration
```

**验证系统时钟：**
- SYSCLK = 80 MHz（通过 PLL）
- HCLK = 80 MHz
- PCLK1 = 80 MHz
- PCLK2 = 80 MHz

**ADC 时钟检查：**
- ADC 时钟来自 APB2（PCLK2）
- 确保 ADC 时钟 ≤ 40 MHz（STM32L4 规格）
- 如果 PCLK2=80MHz，ADC 可能有预分频，检查 ADC 配置页面的时钟设置

---

### 5. GPIO 配置（可选检查）

#### 5.1 PC0 引脚状态
```
Path: Pinout & Configuration → 点击 PC0 引脚
```

**验证：**
- ✅ 引脚标签显示 **ADC1_IN1**
- ✅ 引脚颜色为黄色（模拟功能）
- ✅ 没有配置为 GPIO 输出或输入

---

## ✅ 配置验证清单

在生成代码之前，请逐项检查：

### ADC1 检查
- [ ] ADC1 Channel 1 已启用
- [ ] 引脚映射为 PC0
- [ ] Continuous Conversion Mode = **Disable**
- [ ] External Trigger Source = **Timer 2 Trigger Out**
- [ ] External Trigger Edge = **Trigger Rising Edge**
- [ ] DMA Continuous Requests = **Enable**

### TIM2 检查
- [ ] Prescaler (PSC) = **0**
- [ ] Counter Period (ARR) = **4999**
- [ ] Counter Mode = **Up**
- [ ] TRGO = **Update Event**

### DMA 检查
- [ ] DMA 请求包含 **ADC1**
- [ ] Mode = **Circular**
- [ ] Data Width = **Half Word** (两边都是)
- [ ] Priority = **High** 或 **Very High**

### NVIC 检查
- [ ] DMA1_Channel1 中断已启用

### 时钟检查
- [ ] 系统时钟 = 80 MHz
- [ ] ADC 时钟 ≤ 40 MHz

---

## 🚨 常见配置错误及解决方案

### 错误 1：采样率不正确
**现象：** 实际采样率不是 16kHz

**检查：**
1. TIM2 的 PSC 是否为 0
2. TIM2 的 ARR 是否为 4999
3. 系统时钟是否真的是 80MHz

**解决：** 重新检查 TIM2 配置，确保公式正确

---

### 错误 2：ADC 不触发
**现象：** DMA 缓冲区数据不更新

**检查：**
1. ADC 的 External Trigger 是否启用
2. Trigger Source 是否选择 Timer 2
3. TIM2 的 TRGO 是否为 Update Event

**解决：** 确保 ADC 外部触发和 TIM2 TRGO 正确连接

---

### 错误 3：DMA 只传输一次
**现象：** 采样一次后停止

**检查：**
- DMA Mode 是否为 **Circular**

**解决：** 改为 Circular 模式

---

### 错误 4：数据宽度不匹配
**现象：** ADC 数据读取错误

**检查：**
- DMA Peripheral Data Width 是否为 Half Word
- DMA Memory Data Width 是否为 Half Word
- ADC Resolution 是否为 12 Bits

**解决：** 统一配置为 16 位（Half Word）

---

### 错误 5：PC0 引脚配置错误
**现象：** 读取到随机电平值

**检查：**
- PC0 是否配置为 ADC1_IN1
- 是否误配置为 GPIO

**解决：** 在 Pinout 视图中重新选择 ADC1 → Channel 1

---

##  生成代码后的验证

### 1. 检查生成的初始化代码

打开 `main.c`，确认以下初始化顺序：

```c
// 正确的初始化顺序
MX_GPIO_Init();
MX_DMA_Init();
MX_SPI3_Init();
MX_ADC1_Init();    // ADC 初始化
MX_TIM2_Init();    // 定时器初始化
```

### 2. 检查 ADC 初始化代码

打开 `adc.c`，查找 `MX_ADC1_Init()` 函数，确认：

```c
// 外部触发配置
hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T2_TRGO;
hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;

// DMA 配置
hadc1.Init.DMAContinuousRequests = ENABLE;
```

如果看到 `ADC_SOFTWARE_START`，说明 CubeMX 配置未正确应用，需重新生成代码。

### 3. 检查 TIM2 初始化代码

打开 `tim.c`，查找 `MX_TIM2_Init()` 函数，确认：

```c
htim2.Init.Prescaler = 0;
htim2.Init.CounterPeriod = 4999;
htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
```

### 4. 检查 DMA 初始化代码

打开 `dma.c`，确认 DMA 通道配置正确：
- 方向：Peripheral to Memory
- 模式：Circular
- 数据宽度：Half Word

---

## 🔧 代码修改提示

生成代码后，需要在 `main.c` 中添加：

```c
/* USER CODE BEGIN Includes */
#include "audio_detect.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
AUDIO_Init();
AUDIO_Start();
/* USER CODE END 2 */
```

**注意：** 所有用户代码都应写在 `/* USER CODE BEGIN */` 和 `/* USER CODE END */` 之间，这样重新生成代码时不会丢失。

---

## 📝 调试建议

### 1. 验证 DMA 中断是否触发

在 `main.c` 中添加测试代码：

```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        // 切换 LED 测试中断频率
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        
        // 调用音频处理函数
        AUDIO_Process(...);
    }
}
```

如果 LED 闪烁，说明 DMA 中断正常工作。

### 2. 验证采样率

使用示波器或逻辑分析仪：
- 测量 PC0 引脚（如果配置为定时器输出）
- 或测量 LED 闪烁频率（应为 8kHz，因为每次全传输中断）

### 3. 串口打印 ADC 值

```c
#if DEBUG_ENABLE
printf("ADC=%d, Noise=%d, Amp=%d\r\n", 
       adc_buffer[0], AudioDetect.noise_floor, AudioDetect.amplitude);
#endif
```

观察串口输出，验证 ADC 值在合理范围（1000-3000，中心约 2048）。

---

## 🎯 最终验证

完成所有配置后，系统应满足：

1. ✅ ADC 以精确的 16kHz 采样率连续工作
2. ✅ DMA 自动传输数据，无需 CPU 干预
3. ✅ 每 64 点触发一次中断（4ms 间隔）
4. ✅ 缓冲区数据实时更新
5. ✅ 打响指时能检测到幅度突变
6. ✅ LCD 在检测到时切换显示

---

## 📞 故障排查流程

```
问题：没有检测到声音
  ↓
1. 检查 ADC 是否在工作 → 查看 DMA 中断是否触发
  ↓
2. 检查 ADC 值是否变化 → 串口打印 adc_buffer[0]
  ↓
3. 检查 MAX9814 供电 → 测量 VCC 是否 3.3V
  ↓
4. 检查麦克风连接 → 确认 OUT 接 PC0
  ↓
5. 调整阈值 → 降低 DELTA_THRESHOLD 到 100 测试

问题：LCD 不显示
  ↓
1. 检查 ST7735 初始化 → 确认 SPI 配置正确
  ↓
2. 检查显示函数调用 → 确认 AUDIO_Process 中调用了 LCD_ShowSoundDetected
  ↓
3. 检查显示超时逻辑 → 查看 DisplayActive 标志
```

---

## 📚 参考资源

- STM32L4 Reference Manual (RM0351)
- STM32CubeMX User Manual
- MAX9814 Datasheet
- ST7735 Display Datasheet

---

**祝您配置顺利！如有问题，请参考本文档逐项检查。** 🚀
