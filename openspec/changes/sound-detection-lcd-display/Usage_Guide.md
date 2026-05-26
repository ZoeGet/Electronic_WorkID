# 声音检测功能 - 使用说明

本文档说明如何使用声音检测功能的标志位查询接口。

---

## 📋 架构概述

系统采用**中断驱动 + 主循环查询**的混合架构：

```
┌─────────────────────────────────────────┐
│           中断（实时处理）               │
│  ┌─────────────────────────────────┐    │
│  │ DMA 中断 (每 4ms)                │    │
│  │  1. 采集 64 点 ADC 数据           │    │
│  │  2. 计算幅度、更新底噪          │    │
│  │  3. 检测声音事件                │    │
│  │  4. 触发 LCD 显示                │    │
│  │  5. 设置标志位 AudioStatus      │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
                    ↓ 标志位
┌─────────────────────────────────────────┐
│        主循环（业务逻辑）                │
│  ┌─────────────────────────────────┐    │
│  │ while(1)                        │    │
│  │  1. LED_Process()               │    │
│  │  2. AUDIO_GetStatus()           │    │
│  │  3. 查询 SoundEvent 标志         │    │
│  │  4. 执行业务逻辑                 │    │
│  │  5. 清除标志位                   │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

---

## 🔧 使用方法

### 1. 基本查询（已实现）

在 `main.c` 的 `while(1)` 循环中：

```c
while (1)
{
  LED_Process();
  
  // 查询音频状态
  AUDIO_GetStatus(&AudioStat);
  
  // 检测新的声音事件
  if (AudioStat.SoundEvent == 1)
  {
    // 清除事件标志
    AUDIO_ClearSoundEvent();
    
    // 执行业务逻辑
    // ...
  }
}
```

---

### 2. 状态结构说明

```c
typedef struct {
    __IO uint8_t SoundEvent;        // 声音事件标志
                                    // 1=检测到声音
                                    // 0=未检测到
    
    __IO uint32_t TriggerCount;     // 累计触发次数
                                    // 每次检测到声音 +1
    
    __IO uint32_t LastAmplitude;    // 上次检测到的幅度值
                                    // 范围：0-4095（12 位 ADC）
    
    __IO uint32_t NoiseFloor;       // 当前底噪值
                                    // 范围：0-4095
    
    __IO uint8_t DisplayActive;     // 显示激活标志
                                    // 1=正在显示"Sound Detected"
                                    // 0=正常显示
} AudioStatusTypeDef;
```

---

### 3. 可用 API 函数

#### AUDIO_GetStatus()
```c
// 获取音频状态
AudioStatusTypeDef AudioStat;
AUDIO_GetStatus(&AudioStat);

// 访问各个字段
uint8_t event = AudioStat.SoundEvent;
uint32_t count = AudioStat.TriggerCount;
uint32_t amp = AudioStat.LastAmplitude;
uint32_t noise = AudioStat.NoiseFloor;
```

#### AUDIO_ClearSoundEvent()
```c
// 清除声音事件标志
AUDIO_ClearSoundEvent();
```

#### AUDIO_GetTriggerCount()
```c
// 直接获取触发次数
uint32_t count = AUDIO_GetTriggerCount();
```

---

## 💡 应用示例

### 示例 1：统计触发次数

```c
/* USER CODE BEGIN PV */
AudioStatusTypeDef AudioStat;
uint32_t LastCount = 0;
/* USER CODE END PV */

while (1)
{
  AUDIO_GetStatus(&AudioStat);
  
  if (AudioStat.TriggerCount > LastCount)
  {
    printf("第 %lu 次声音触发\r\n", AudioStat.TriggerCount);
    LastCount = AudioStat.TriggerCount;
  }
}
```

---

### 示例 2：根据幅度执行不同操作

```c
while (1)
{
  AUDIO_GetStatus(&AudioStat);
  
  if (AudioStat.SoundEvent == 1)
  {
    AUDIO_ClearSoundEvent();
    
    // 根据幅度判断声音强度
    if (AudioStat.LastAmplitude > 500)
    {
      printf("强声音触发！\r\n");
      // 执行强声音对应的操作
    }
    else if (AudioStat.LastAmplitude > 200)
    {
      printf("中等声音触发\r\n");
      // 执行中等声音对应的操作
    }
    else
    {
      printf("轻微声音触发\r\n");
      // 执行轻微声音对应的操作
    }
  }
}
```

---

### 示例 3：定时上报状态

```c
while (1)
{
  static uint32_t last_report = 0;
  
  // 每秒上报一次状态
  if (HAL_GetTick() - last_report > 1000)
  {
    AUDIO_GetStatus(&AudioStat);
    printf("Status: Count=%lu, Amp=%lu, Noise=%lu, Display=%d\r\n",
           AudioStat.TriggerCount,
           AudioStat.LastAmplitude,
           AudioStat.NoiseFloor,
           AudioStat.DisplayActive);
    last_report = HAL_GetTick();
  }
  
  // 处理声音事件
  if (AudioStat.SoundEvent == 1)
  {
    AUDIO_ClearSoundEvent();
    // ...
  }
}
```

---

### 示例 4：与其他模块联动

```c
while (1)
{
  LED_Process();
  
  AUDIO_GetStatus(&AudioStat);
  
  if (AudioStat.SoundEvent == 1)
  {
    AUDIO_ClearSoundEvent();
    
    // 联动示例：
    
    // 1. 切换 LED 模式
    LED_SetMode(LED_MODE_BLINK);
    
    // 2. 发送蓝牙通知
    #ifdef USE_BLE
    BLE_SendNotification("Sound detected!");
    #endif
    
    // 3. 记录日志到 SD 卡
    #ifdef USE_SD
    SD_LogEvent("SOUND", AudioStat.LastAmplitude);
    #endif
    
    // 4. 蜂鸣器提示
    #ifdef USE_BUZZER
    BEEP_Short();
    #endif
  }
}
```

---

### 示例 5：多级触发（根据底噪动态判断）

```c
while (1)
{
  AUDIO_GetStatus(&AudioStat);
  
  if (AudioStat.SoundEvent == 1)
  {
    AUDIO_ClearSoundEvent();
    
    // 计算信噪比
    uint32_t snr = AudioStat.LastAmplitude / 
                   (AudioStat.NoiseFloor > 0 ? AudioStat.NoiseFloor : 1);
    
    if (snr > 5)  // 信噪比 > 5
    {
      printf("清晰的声音触发 (SNR=%lu)\r\n", snr);
      // 执行高优先级操作
    }
    else if (snr > 2)  // 信噪比 > 2
    {
      printf("普通声音触发 (SNR=%lu)\r\n", snr);
      // 执行普通操作
    }
    else
    {
      printf("微弱声音触发 (SNR=%lu)\r\n", snr);
      // 执行低优先级操作或忽略
    }
  }
}
```

---

## 📊 调试输出

### 启用调试输出

在 `audio_detect.h` 中设置：

```c
#define DEBUG_ENABLE    1   // 1=启用，0=禁用
```

### 串口输出内容

**中断中的输出（检测到声音时）：**
```
Sound Detected! Amp=450, Threshold=320, Noise=120, Count=1
```

**主循环中的输出（已实现）：**
```
Main: Sound Event #1, Amp=450, Noise=120
```

**定时状态上报（需手动添加）：**
```
Status: Count=5, Amp=380, Noise=115, Display=0
```

---

## ⚙️ 参数调优

### 调整触发阈值

在 `audio_detect.h` 中修改：

```c
#define DELTA_THRESHOLD     200   // 建议范围：150-300
```

**调优步骤：**

1. 启用串口调试输出
2. 打开串口助手观察 `Noise` 值（底噪）
3. 打响指，观察 `Amp` 值（幅度）
4. 设置 `DELTA_THRESHOLD = (Amp - Noise) * 0.7`

**示例：**
- 安静时底噪 Noise = 120
- 打响指幅度 Amp = 450
- 差值 = 330
- 设置 `DELTA_THRESHOLD = 330 * 0.7 ≈ 230`

---

### 调整冷却时间

```c
#define COOLDOWN_MS         500   // 建议范围：300-1000ms
```

**影响：**
- 值越小 → 响应越快，但可能重复触发
- 值越大 → 防抖越好，但可能漏掉快速连续的声音

---

### 调整显示时长

```c
#define DISPLAY_TIMEOUT_MS  3000  // 建议范围：2000-5000ms
```

**影响：**
- 显示"Sound Detected"的持续时间
- 根据用户体验调整

---

## 🔍 故障排查

### 问题 1：主循环查询不到事件

**检查：**
1. 确认 `AUDIO_GetStatus()` 已调用
2. 检查 `AudioStat.SoundEvent` 是否为 1
3. 确认中断正常工作（查看 DMA 中断标志）

**解决：**
```c
// 添加调试代码
if (AudioStat.SoundEvent == 0)
{
  printf("No event detected\r\n");
}
```

---

### 问题 2：触发次数不增加

**检查：**
1. 查看中断输出是否有 "Count=X"
2. 检查 `LastTriggerCount` 是否正确更新
3. 确认阈值设置合理

**解决：**
```c
// 打印详细状态
printf("Stat: Event=%d, Count=%lu, LastCount=%lu\r\n",
       AudioStat.SoundEvent,
       AudioStat.TriggerCount,
       LastTriggerCount);
```

---

### 问题 3：幅度值异常

**正常范围：**
- 安静时：Amp < 50
- 说话时：Amp = 100-300
- 拍掌时：Amp = 300-600
- 喊叫时：Amp > 600

**如果幅度值始终很小：**
1. 检查 MAX9814 供电（应为 3.3V）
2. 检查麦克风连接
3. 尝试降低 `DELTA_THRESHOLD`

**如果幅度值饱和（接近 4095）：**
1. 降低 MAX9814 增益（GAIN 引脚接地=40dB）
2. 减小声音强度

---

## 📝 最佳实践

### 1. 主循环处理原则

✅ **推荐在主循环中做：**
- 标志位查询
- 业务逻辑处理
- 通信协议处理
- 复杂计算
- 串口打印

❌ **不推荐在主循环中做：**
- 延时等待（使用定时器）
- 阻塞式 I/O
- 高频轮询（>1kHz）

---

### 2. 标志位处理原则

```c
// ✅ 正确：先查询，再清除，后处理
if (AudioStat.SoundEvent == 1)
{
  AUDIO_ClearSoundEvent();  // 立即清除
  // 处理业务逻辑...
}

// ❌ 错误：先处理，很久后才清除
if (AudioStat.SoundEvent == 1)
{
  // 处理复杂逻辑...
  // 可能耗时很久
  AUDIO_ClearSoundEvent();  // 清除太晚
}
```

---

### 3. 状态结构使用

```c
// ✅ 推荐：局部变量缓存
AudioStatusTypeDef Stat;
AUDIO_GetStatus(&Stat);

if (Stat.SoundEvent == 1)
{
  // 使用缓存的 Stat
}

// ❌ 不推荐：多次调用全局变量
if (AudioStatus.SoundEvent == 1)
{
  // 如果中断同时修改，可能不一致
}
```

---

## 🎯 扩展功能建议

### 1. 添加声音强度分级

```c
typedef enum {
  SOUND_LEVEL_NONE = 0,
  SOUND_LEVEL_LOW,
  SOUND_LEVEL_MEDIUM,
  SOUND_LEVEL_HIGH
} SoundLevelTypeDef;

SoundLevelTypeDef GetSoundLevel(uint32_t amplitude)
{
  if (amplitude < 150) return SOUND_LEVEL_NONE;
  if (amplitude < 350) return SOUND_LEVEL_LOW;
  if (amplitude < 600) return SOUND_LEVEL_MEDIUM;
  return SOUND_LEVEL_HIGH;
}
```

---

### 2. 添加声音事件队列

```c
// 记录最近 10 次声音事件
typedef struct {
  uint32_t timestamp[10];
  uint32_t amplitude[10];
  uint8_t head;
  uint8_t count;
} SoundEventQueueTypeDef;

SoundEventQueueTypeDef SoundQueue;

void AddEventToQueue(uint32_t amp)
{
  SoundQueue.timestamp[SoundQueue.head] = HAL_GetTick();
  SoundQueue.amplitude[SoundQueue.head] = amp;
  SoundQueue.head = (SoundQueue.head + 1) % 10;
  if (SoundQueue.count < 10) SoundQueue.count++;
}
```

---

### 3. 添加长时间静默检测

```c
// 检测超过 10 秒无声音
static uint32_t LastSoundTime = 0;

if (AudioStat.SoundEvent == 1)
{
  LastSoundTime = HAL_GetTick();
}

if (HAL_GetTick() - LastSoundTime > 10000)
{
  printf("长时间静默检测\r\n");
}
```

---

## 📚 参考资源

- `audio_detect.h` - 头文件，查看 API 声明
- `audio_detect.c` - 实现文件，查看内部逻辑
- `CubeMX_Configuration_Guide.md` - 硬件配置指南
- `openspec/changes/sound-detection-lcd-display/` - OpenSpec 文档

---

**祝您使用愉快！** 🎉

如有问题，请参考本文档或查看调试输出。
