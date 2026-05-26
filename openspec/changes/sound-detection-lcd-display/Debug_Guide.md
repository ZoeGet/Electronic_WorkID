# 声音检测调试指南

## 🔍 当前状态分析

### 显示信息
```
Debug:
StdDev:000      ← 标准差为 0
Thres :010      ← 阈值=10
BufRdy:0        ← ⚠️ 缓冲区就绪标志一直为 0（问题所在！）
Count :00000    ← 触发次数为 0
```

### 问题诊断

**BufRdy 一直为 0** 说明：
- ❌ DMA 中断没有触发
- ❌ 或 DMA 没有传输数据
- ❌ 或 ADC 没有启动

---

## ✅ 解决步骤

### 步骤 1：检查 CubeMX 配置

#### 1.1 检查 ADC1 DMA 配置

打开 `.ioc` 文件，检查：

**Path:** Configuration → DMA

- [ ] DMA 请求列表中有 **ADC1**
- [ ] Mode = **Circular**（循环模式）⭐重要
- [ ] Data Width = **Half Word**
- [ ] Priority = **High** 或 **Very High**

**如果 Mode 不是 Circular：**
1. 点击 DMA 配置
2. 将 Mode 改为 **Circular**
3. 重新生成代码

---

#### 1.2 检查 ADC1 配置

**Path:** Configuration → Analog → ADC1 → Parameter Settings

- [ ] External Trigger Conversion Source = **Timer 2 Trigger Out**
- [ ] External Trigger Conversion Edge = **Trigger Rising Edge**
- [ ] DMA Continuous Requests = **Enable** ⭐重要
- [ ] Continuous Conversion Mode = **Disable**

**如果 DMA Continuous Requests 未启用：**
1. 勾选 **DMA Continuous Requests**
2. 重新生成代码

---

#### 1.3 检查 TIM2 配置

**Path:** Configuration → Timers → TIM2 → Parameter Settings

- [ ] Prescaler (PSC) = **0**
- [ ] Counter Period (ARR) = **4999**
- [ ] Trigger Event Selection (TRGO) = **Update Event** ⭐重要

**如果 TRGO 不是 Update Event：**
1. 展开 Trigger Output (TRGO) Parameters
2. 选择 **Update Event**
3. 重新生成代码

---

#### 1.4 检查 NVIC 配置

**Path:** Configuration → System Core → NVIC

- [ ] DMA1_Channel1_IRQn = **Enabled** ⭐重要

**如果未启用：**
1. 勾选 **DMA1_Channel1 global interrupt**
2. 重新生成代码

---

### 步骤 2：验证启动顺序

确保 main.c 中的启动顺序正确：

```c
/* USER CODE BEGIN 2 */
ST7735_Init(); 
LED_Init();

/* 初始化声音检测器 */
SoundDetector_Init(&gSoundDetector, adc_buffer1, adc_buffer2, ADC_BUFFER_SIZE);

/* 启动 TIM2 触发 ADC */
HAL_TIM_Base_Start(&htim2);

/* 启动 ADC 连续转换（TIM2 硬件触发 + DMA 双缓冲） */
HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer1, ADC_BUFFER_SIZE);

/* 初始化 LCD 显示 */
LCD_DisplayInit();
/* USER CODE END 2 */
```

**关键点：**
1. ✅ 先初始化 SoundDetector
2. ✅ 再启动 TIM2
3. ✅ 最后启动 ADC+DMA

---

### 步骤 3：测试 DMA 中断

#### 方法 1：观察 LED

如果 DMA 中断正常触发，LED 应该会闪烁（PB13）。

**如果 LED 不亮：**
- DMA 中断没有触发
- 检查 NVIC 配置

---

#### 方法 2：观察 BufRdy

**正常情况：** BufRdy 应该在 0→1→2→0 循环变化

**如果一直是 0：**
1. TIM2 没有触发 ADC
2. ADC 没有启动 DMA
3. DMA 配置错误

**如果偶尔变化：**
1. 配置基本正确
2. 可能是中断优先级问题

---

### 步骤 4：检查 ADC 数据

在 `SoundDetector_Process()` 函数中添加调试：

```c
bool SoundDetector_Process(SoundDetector_t* det)
{
  if (det->bufferReady == 0) {
    return false;
  }
  
  /* 选择当前缓冲区 */
  uint16_t* currentBuffer = (det->bufferReady == 1) ? det->buffer1 : det->buffer2;
  
  /* 调试：检查缓冲区第一个值 */
  // 在 LCD 上显示 adc_buffer1[0] 的值
  uint16_t testValue = currentBuffer[0];
  
  /* 计算标准差 */
  uint32_t stdDev = SoundDetector_CalculateStdDev(currentBuffer, det->bufferSize / 2, NULL);
  det->lastStdDev = stdDev;
  
  // ... 其余代码
}
```

**正常 ADC 值：**
- 应该在 1000-3000 之间（中心约 2048）
- 如果一直是 0 或 65535，说明 ADC 没有正常工作

---

### 步骤 5：检查 MAX9814 连接

#### 硬件检查

- [ ] MAX9814 VCC = 3.3V
- [ ] MAX9814 GND = GND
- [ ] MAX9814 OUT = PC0 (ADC1_IN1)
- [ ] GAIN 悬空（50dB）
- [ ] AR 悬空（自动增益）

#### 测试方法

1. 用万用表测量 MAX9814 VCC 引脚
2. 用示波器观察 OUT 引脚（应该有波形）
3. 对着麦克风说话，观察 StdDev 是否变化

---

## 🎯 预期结果

### 正常运行时

```
BufRdy: 0 → 1 → 2 → 0 → 1 → 2 ... (快速循环)
StdDev: 010-050 (底噪范围)
Thres : 010
Count : 00000 (安静时)
```

### 打响指时

```
BufRdy: 1 或 2
StdDev: 100-500 (瞬间增大)
Thres : 010
Count : 00001 (递增)
```

---

## ⚠️ 常见问题

### 问题 1：BufRdy 一直为 0

**原因：** DMA 没有工作

**解决：**
1. 检查 DMA Mode 是否为 Circular
2. 检查 DMA Continuous Requests 是否启用
3. 检查 NVIC 中 DMA1_Channel1 中断是否启用
4. 重新生成 CubeMX 代码

---

### 问题 2：StdDev 一直为 0

**原因：** ADC 缓冲区数据为 0

**解决：**
1. 检查 ADC 是否启动：`HAL_ADC_Start_DMA()`
2. 检查 TIM2 是否触发 ADC
3. 检查 PC0 引脚连接

---

### 问题 3：BufRdy 变化但 StdDev 不变

**原因：** MAX9814 没有信号输出

**解决：**
1. 检查 MAX9814 供电
2. 检查麦克风连接
3. 对着麦克风说话测试

---

### 问题 4：Count 不递增

**原因：** StdDev < Threshold

**解决：**
```c
// 降低阈值测试
#define SOUND_THRESHOLD    5U

// 或在运行时调整
SoundDetector_SetThreshold(&gSoundDetector, 5);
```

---

## 📝 快速测试代码

如果想快速测试，可以临时修改阈值：

```c
/* 在 main.c 的 while(1) 中 */
while (1)
{
  LED_Process();
  
  /* 强制降低阈值测试 */
  gSoundDetector.threshold = 5;  // 最低阈值测试
  
  /* 其余代码保持不变 */
  // ...
}
```

**观察：**
- 如果 Count 开始递增，说明之前阈值太高
- 如果 Count 仍为 0，说明 StdDev 一直为 0

---

## 🔧 最终检查清单

- [ ] CubeMX 配置正确（DMA Circular、TRGO=Update、NVIC 启用）
- [ ] 重新生成代码
- [ ] 编译无错误
- [ ] 下载程序
- [ ] BufRdy 循环变化（0→1→2→0）
- [ ] StdDev 有数值（10-50）
- [ ] 打响指时 StdDev 增大
- [ ] Count 递增
- [ ] LCD 切换显示

---

**如果按照以上步骤仍然无法解决，请提供：**
1. BufRdy 的实际值
2. StdDev 的实际值
3. LED 是否闪烁
4. CubeMX 的 DMA 配置截图

**这样可以更精确地诊断问题！** 🔍
