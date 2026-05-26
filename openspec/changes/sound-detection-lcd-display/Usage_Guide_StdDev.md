# 声音检测功能 - 使用说明（标准差算法）

本文档说明基于 Smart_Card 项目的声音检测功能，使用**标准差算法**检测声音。

---

## 📋 算法原理

### 标准差检测法

通过分析 ADC 采样数据的**离散程度**来判断是否有声音：

```
1. 计算平均值：mean = sum(buffer) / size
2. 计算方差：variance = sum((sample - mean)²) / size  
3. 计算标准差：stdDev = sqrt(variance)
4. 判断：如果 stdDev > threshold，则检测到声音
```

**优势：**
- ✅ 不受直流偏置影响（MAX9814 输出有 1.6V 偏置）
- ✅ 计算简单快速
- ✅ 对噪声不敏感
- ✅ Smart_Card 项目已验证

---

## 🔧 配置参数

### 关键参数（在 `audio_detect.h` 中）

```c
#define SOUND_BUFFER_SIZE       128U    // DMA 缓冲区大小
#define SOUND_THRESHOLD         15U     // 声音检测阈值（标准差）⭐重要
#define SOUND_DISPLAY_TIME    3000U     // 显示持续时间（ms）
#define SOUND_COOLDOWN_TIME    500U     // 冷却时间（ms）
```

### 阈值调优指南

| 阈值 | 适用场景 | 说明 |
|------|---------|------|
| 10-15 | 安静环境 | 灵敏度高，轻微声音即可触发 |
| 15-25 | 普通环境 | 推荐初始值，平衡灵敏度和抗干扰 |
| 25-40 | 嘈杂环境 | 降低灵敏度，避免误触发 |

**调优步骤：**
1. 启用串口调试（`DEBUG_ENABLE=1`）
2. 打开串口助手观察 `StdDev` 值
3. 安静时观察底噪标准差（如 5-8）
4. 打响指观察标准差（如 30-50）
5. 设置阈值 = (底噪 + 峰值) / 2

---

## 📊 串口调试输出

### 启用调试输出

在 `audio_detect.h` 中设置：
```c
#define DEBUG_ENABLE    1   // 1=启用，0=禁用
```

### 输出示例

**检测到声音时：**
```
Sound Detected! StdDev=35, Threshold=15, Count=1
LCD: Sound Detected @ 1234 ms, StdDev=35
```

**主循环查询：**
```
Main: Sound Event #1, StdDev=35
```

**显示恢复：**
```
LCD: Page Restored
```

---

## 💡 使用方法

### 基本查询（已实现）

```c
// 在 main.c 的 while(1) 中
AUDIO_GetStatus(&AudioStat);

if (AudioStat.SoundEvent == 1)
{
  AUDIO_ClearSoundEvent();
  
  // 业务逻辑...
}
```

### 状态结构

```c
typedef struct {
    __IO uint8_t SoundEvent;        // 声音事件标志
    __IO uint32_t TriggerCount;     // 累计触发次数
    __IO uint32_t LastStdDev;       // 上次标准差值（信号强度）
    __IO uint8_t DisplayActive;     // 显示激活标志
} AudioStatusTypeDef;
```

---

## 🎯 应用示例

### 示例 1：根据信号强度分级

```c
if (AudioStat.SoundEvent == 1)
{
  AUDIO_ClearSoundEvent();
  
  // 根据标准差判断声音强度
  if (AudioStat.LastStdDev > 40)
  {
    printf("强声音触发！\r\n");
    // 强声音操作
  }
  else if (AudioStat.LastStdDev > 20)
  {
    printf("中等声音\r\n");
    // 中等声音操作
  }
  else
  {
    printf("轻微声音\r\n");
    // 轻微声音操作
  }
}
```

### 示例 2：动态阈值调整

```c
// 根据环境噪声自动调整阈值
static uint32_t ambientStdDev = 0;

if (AudioStat.LastStdDev > 0)
{
  // 低通滤波跟踪环境噪声
  ambientStdDev = ambientStdDev * 0.95 + AudioStat.LastStdDev * 0.05;
  
  // 动态阈值 = 环境噪声 + 偏移
  uint32_t dynamicThreshold = ambientStdDev + 10;
  AUDIO_SetThreshold(dynamicThreshold);
}
```

### 示例 3：统计触发频率

```c
static uint32_t lastCount = 0;
static uint32_t triggerRate = 0;

if (AudioStat.TriggerCount > lastCount)
{
  triggerRate = AudioStat.TriggerCount - lastCount;
  lastCount = AudioStat.TriggerCount;
  
  if (triggerRate > 5)
  {
    printf("频繁触发警告！\r\n");
    // 降低灵敏度
    AUDIO_SetThreshold(AudioDetect.threshold + 5);
  }
}
```

---

## 🔍 故障排查

### 问题 1：一直触发（误触发）

**现象：** 没有声音也持续触发

**原因：** 阈值设置过低

**解决：**
```c
// 提高阈值
#define SOUND_THRESHOLD    25U  // 从 15 提高到 25
```

或运行时调整：
```c
AUDIO_SetThreshold(25);
```

---

### 问题 2：不触发（灵敏度低）

**现象：** 大声喊叫也不触发

**原因：** 阈值设置过高

**解决：**
```c
// 降低阈值
#define SOUND_THRESHOLD    10U  // 从 15 降低到 10
```

或运行时调整：
```c
AUDIO_SetThreshold(10);
```

---

### 问题 3：标准差值异常

**正常范围：**
- 安静时：StdDev = 3-10
- 说话时：StdDev = 15-30
- 拍掌时：StdDev = 30-60
- 喊叫时：StdDev = 50-100

**如果值始终很小：**
1. 检查 MAX9814 供电（3.3V）
2. 检查麦克风连接
3. 降低阈值测试

**如果值饱和（接近 4095）：**
1. 降低 MAX9814 增益（GAIN 引脚）
2. 减小声音强度

---

## ⚙️ 参数调优流程

### 步骤 1：测量环境底噪

```c
// 安静环境下运行，串口观察 StdDev 值
// 假设底噪 StdDev = 5-8
```

### 步骤 2：测试触发声音

```c
// 打响指，观察 StdDev 峰值
// 假设峰值 StdDev = 35-50
```

### 步骤 3：设置阈值

```c
// 阈值 = (底噪 + 峰值) / 2
// 例如：(8 + 40) / 2 = 24
#define SOUND_THRESHOLD    24U
```

### 步骤 4：微调

- 如果误触发 → 增加阈值 2-3
- 如果不灵敏 → 降低阈值 2-3

---

## 📈 算法对比

### 峰值 - 峰值法（旧方案）

```c
amplitude = max(buffer) - min(buffer);
if (amplitude > threshold) { /* 触发 */ }
```

**缺点：**
- ❌ 受直流偏置影响大
- ❌ 对单个异常值敏感
- ❌ 需要复杂的底噪跟踪

### 标准差法（新方案）⭐

```c
stdDev = sqrt(sum((sample - mean)²) / size);
if (stdDev > threshold) { /* 触发 */ }
```

**优点：**
- ✅ 自动消除直流偏置
- ✅ 统计平均，抗干扰强
- ✅ 无需复杂底噪跟踪
- ✅ Smart_Card 项目验证

---

## 🎛️ API 参考

### 初始化
```c
void AUDIO_Init(void);
void AUDIO_Start(void);
```

### 状态查询
```c
void AUDIO_GetStatus(AudioStatusTypeDef* status);
uint8_t AUDIO_IsSoundDetected(void);
uint32_t AUDIO_GetTriggerCount(void);
```

### 参数控制
```c
void AUDIO_SetThreshold(uint32_t threshold);
uint32_t AUDIO_GetStdDev(void);
void AUDIO_ResetDetection(void);
```

---

## 📝 最佳实践

### 1. 阈值设置

```c
// ✅ 推荐：根据实测调整
#define SOUND_THRESHOLD    15U  // 初始值，实测后调整

// ❌ 不推荐：随意设置
#define SOUND_THRESHOLD    100U  // 太高，可能不触发
```

### 2. 主循环处理

```c
// ✅ 推荐：及时清除标志
if (AudioStat.SoundEvent == 1)
{
  AUDIO_ClearSoundEvent();
  // 处理业务
}

// ❌ 不推荐：不清除标志
if (AudioStat.SoundEvent == 1)
{
  // 处理业务
  // 忘记清除标志
}
```

### 3. 调试输出

```c
// ✅ 推荐：开发阶段启用
#define DEBUG_ENABLE    1

// 量产阶段关闭
#define DEBUG_ENABLE    0  // 节省代码空间
```

---

## 📚 参考资源

- `audio_detect.h` - API 头文件
- `audio_detect.c` - 实现源码（标准差算法）
- `../Smart_Card/sound_detector.c` - 参考实现
- `CubeMX_Configuration_Guide.md` - 硬件配置

---

## 🚀 快速开始

1. **配置 CubeMX**（参考配置指南）
2. **生成代码**
3. **编译下载**
4. **串口调试**（波特率 115200）
5. **观察 StdDev 值**
6. **调整阈值**
7. **测试打响指**

---

**祝您使用愉快！** 🎉

如有问题，请观察串口输出调整阈值。
