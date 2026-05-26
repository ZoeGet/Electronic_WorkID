# 声音检测功能实现说明

## ✅ 已完成的实现

### 1. 核心文件

**audio_detect.c** - 声音检测算法实现
- `SoundDetector_Init()` - 初始化检测器
- `SoundDetector_Process()` - 处理音频数据
- `SoundDetector_CalculateStdDev()` - 计算标准差（信号能量）
- `SoundDetector_IsDetected()` - 检查检测状态
- `HAL_ADC_ConvHalfCpltCallback()` - DMA 半传输中断
- `HAL_ADC_ConvCpltCallback()` - DMA 全传输中断

**audio_detect.h** - 头文件
- `SoundDetector_t` 结构体定义
- 函数声明
- 配置参数

### 2. 工作原理

```
1. TIM2 以 16kHz 频率触发 ADC 采样
2. ADC 采集 MAX9814 输出的音频信号
3. DMA 自动将数据传输到双缓冲区（adc_buffer1/2）
4. 每传输 64 点（半缓冲），触发一次中断
5. 中断中设置 bufferReady 标志
6. 主循环调用 SoundDetector_Process() 处理数据
7. 计算标准差，判断是否超过阈值
8. 如果超过阈值，设置 soundDetected 标志
9. LCD 显示"Sound Detected!"
```

### 3. 数据流

```
MAX9814 → PC0(ADC1_IN1) → ADC1 → DMA1_Channel1 → 双缓冲区
                                              ↓
TIM2(16kHz 触发)                              ↓
                                              ↓
主循环 ← SoundDetector_Process() ← bufferReady 标志
    ↓
LCD_DisplaySoundDetected()
```

## 🔧 配置参数

### 在 audio_detect.h 中

```c
#define SOUND_BUFFER_SIZE       128U    // DMA 缓冲区大小
#define SOUND_THRESHOLD         10U     // 声音检测阈值（标准差）
#define SOUND_DISPLAY_TIME    1000U     // 显示持续时间（ms）
```

### 阈值调优

| 场景 | 阈值 | 说明 |
|------|------|------|
| 安静环境 | 8-12 | 灵敏度高 |
| 普通环境 | 12-20 | 推荐初始值 |
| 嘈杂环境 | 20-30 | 降低灵敏度 |

**调优步骤：**
1. 编译下载程序
2. 打开串口助手（可选）
3. 观察安静时的 StdDev 值
4. 打响指观察峰值
5. 设置阈值 = (底噪 + 峰值) / 2

## 📊 预期效果

### LCD 显示

**等待状态：**
```
┌─────────────────────┐
│ Sound Detector      │
│ 16kHz Sampling      │
│ Waiting...          │
└─────────────────────┘
```

**检测到声音：**
```
┌─────────────────────┐
│ Sound Detected!     │
│      (红色)         │
└─────────────────────┘
```

### 响应时间

- 采样率：16kHz
- 处理延迟：约 4ms（64 点/16kHz）
- 显示响应：< 10ms
- 显示持续：1 秒后自动恢复

## ⚠️ 注意事项

### 1. CubeMX 配置检查

**ADC1 配置：**
- ✅ Channel 1 (PC0) 已启用
- ✅ External Trigger Source = Timer 2 Trigger Out
- ✅ External Trigger Edge = Rising Edge
- ✅ DMA Continuous Requests = Enable
- ✅ Continuous Conversion Mode = Disable

**TIM2 配置：**
- ✅ Prescaler = 0
- ✅ Counter Period = 4999
- ✅ TRGO = Update Event

**DMA 配置：**
- ✅ Mode = Circular
- ✅ Data Width = Half Word
- ✅ DMA1_Channel1 中断已启用

### 2. 硬件连接

- MAX9814 OUT → PC0 (ADC1_IN1)
- MAX9814 VCC → 3.3V
- MAX9814 GND → GND
- GAIN 悬空（50dB）
- AR 悬空（自动增益）

### 3. 常见问题

**问题 1：不触发**
- 检查阈值是否过高
- 检查 MAX9814 供电
- 检查麦克风连接

**问题 2：持续触发**
- 降低阈值
- 检查环境噪声

**问题 3：编译错误**
- 确保包含 audio_detect.h
- 确保 adc.h、tim.h 已包含

## 🚀 使用示例

### 在 main.c 中

```c
/* 初始化 */
SoundDetector_Init(&gSoundDetector, adc_buffer1, adc_buffer2, ADC_BUFFER_SIZE);
HAL_TIM_Base_Start(&htim2);
HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer1, ADC_BUFFER_SIZE);
LCD_DisplayInit();

/* 主循环 */
while (1)
{
  LED_Process();
  
  if (SoundDetector_IsDetected(&gSoundDetector))
  {
    if (!displayState)
    {
      LCD_DisplaySoundDetected();
      displayState = true;
    }
  }
  else
  { 
    if (displayState)
    {
      displayState = false;
      LCD_DisplayWaiting();
    }
  }
  
  SoundDetector_Process(&gSoundDetector);
}
```

### 调整阈值

```c
/* 运行时动态调整 */
if (误触发)
{
  SoundDetector_SetThreshold(&gSoundDetector, 20);  // 提高阈值
}
else if (不灵敏)
{
  SoundDetector_SetThreshold(&gSoundDetector, 8);   // 降低阈值
}
```

## 📈 算法说明

### 标准差计算

```
1. 计算平均值：mean = sum(buffer) / size
2. 计算方差：variance = sum((sample - mean)²) / size
3. 计算标准差：stdDev = sqrt(variance)
4. 判断：如果 stdDev > threshold，则检测到声音
```

**优势：**
- 自动消除直流偏置（MAX9814 的 1.6V 偏置）
- 统计平均，抗干扰强
- 计算简单快速

### 双缓冲机制

```
adc_buffer1 (64 点) ← DMA 传输 → 中断处理
adc_buffer2 (64 点) ← DMA 传输 → 中断处理
```

**优势：**
- 连续采样，无数据丢失
- 后台处理，不影响主循环
- 实时响应

## ✅ 验证清单

- [ ] CubeMX 配置正确（ADC、TIM2、DMA）
- [ ] 生成代码
- [ ] 添加 audio_detect.c/.h 到项目
- [ ] 编译无错误
- [ ] 下载程序
- [ ] LCD 显示初始化界面
- [ ] 打响指测试
- [ ] 观察 LCD 切换显示
- [ ] 调整阈值到最佳

---

**实现完成！可以编译测试了。** 🎉
