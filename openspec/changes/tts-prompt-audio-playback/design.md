## Context

目标 MCU 为 STM32L431 系列，当前工程已初始化以下播放相关外设：

- `DAC1`：输出通道 `DAC_CHANNEL_1`，物理引脚 `PA4 / DAC1_OUT1`
- `TIM6`：作为 DAC 触发源，通过 `TIM_TRGO_UPDATE` 产生采样节拍
- `DMA1_Channel3`：当前用于 DAC 通道 1 的内存到外设搬运

现有系统已经实现 KEY0 开始录音、KEY1 停止录音。新功能要求在这些动作发生时播放 TTS 语音提示，并尽量避免提示音进入录音文件。

## Goals / Non-Goals

**Goals**

- 使用 `DAC1 + TIM6 + DMA_NORMAL` 播放预生成 TTS 语音提示
- 支持至少两条提示音：`开始录音`、`录音已停止`
- 支持错误提示音扩展，例如 `存储错误`
- 播放接口非阻塞，主循环在播放期间仍可继续运行
- KEY0 的开始录音提示音播放完成后再真正启动录音
- KEY1 停止录音后播放停止提示音
- 播放结束后 DAC 输出回到静音中点，避免输出悬空噪声

**Non-Goals**

- 不在 STM32 上实时合成中文 TTS
- 不播放 MP3、AAC、WAV 文件流
- 不实现复杂播放队列
- 不实现音频混音、回声消除、降噪或音量 UI
- 不使用 I2S/SAI 外部音频 Codec

## Architecture

### 1) 音频数据来源

TTS 语音在 PC 上提前生成，并转换为 16kHz、单声道、12-bit DAC 数据数组。

推荐数据格式：

```c
const uint16_t tts_start_record[] = {
    2048, 2052, 2060
};

const uint32_t tts_start_record_len =
    sizeof(tts_start_record) / sizeof(tts_start_record[0]);
```

采样点范围：

- 最小值：`0`
- 最大值：`4095`
- 静音中点：`2048`

若原始音频为 signed 16-bit PCM，转换公式为：

```c
dac_value = (pcm_value + 32768) >> 4;
```

### 2) TIM6 采样时基

TIM6 作为 DAC 触发时钟。目标采样率固定为 16kHz。

当 TIM6 输入时钟为 80MHz 时：

```text
Fs = 80,000,000 / (PSC + 1) / (ARR + 1)
Fs = 80,000,000 / 1 / 5000 = 16,000 Hz
```

推荐参数：

```c
htim6.Init.Prescaler = 0;
htim6.Init.Period = 4999;
```

并保持：

```c
sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
```

### 3) DAC 输出配置

推荐 DAC1 通道 1 配置：

```c
sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
```

最终必须确认 `PA4 / DAC1_OUT1` 可以输出到外部引脚。

### 4) DMA 配置

提示音播放应使用一次性 DMA 传输，而不是循环模式。

推荐配置：

```c
hdma_dac_ch1.Init.Direction = DMA_MEMORY_TO_PERIPH;
hdma_dac_ch1.Init.PeriphInc = DMA_PINC_DISABLE;
hdma_dac_ch1.Init.MemInc = DMA_MINC_ENABLE;
hdma_dac_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
hdma_dac_ch1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
hdma_dac_ch1.Init.Mode = DMA_NORMAL;
hdma_dac_ch1.Init.Priority = DMA_PRIORITY_LOW;
```

如果播放期间与其他 DMA 操作竞争导致声音异常，可将 DAC DMA 优先级调整为 `DMA_PRIORITY_MEDIUM`。

### 5) 播放模块接口

建议新增 `tts_player.h/.c`，对外暴露以下接口：

```c
void TTS_Player_Init(void);
bool TTS_Player_IsBusy(void);
bool TTS_Player_PlayStartRecord(void);
bool TTS_Player_PlayStopRecord(void);
bool TTS_Player_PlayStorageError(void);
void TTS_Player_Stop(void);
```

内部维护忙碌标志：

```c
static volatile bool ttsBusy;
```

播放策略：

- 如果当前空闲，则启动播放
- 如果正在播放，则忽略新的播放请求并返回 `false`
- 播放完成回调中清除 busy 状态

### 6) 播放启动流程

核心流程：

```text
停止 TIM6
停止 DAC DMA
设置 DAC 输出到 2048
启动 DAC DMA
启动 TIM6
```

伪代码：

```c
static bool TTS_Player_Play(const uint16_t *data, uint32_t length)
{
    if (ttsBusy || data == NULL || length == 0U) {
        return false;
    }

    ttsBusy = true;

    HAL_TIM_Base_Stop(&htim6);
    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2048U);

    if (HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1,
                          (uint32_t *)data, length,
                          DAC_ALIGN_12B_R) != HAL_OK) {
        ttsBusy = false;
        return false;
    }

    if (HAL_TIM_Base_Start(&htim6) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
        ttsBusy = false;
        return false;
    }

    return true;
}
```

### 7) 播放完成处理

使用 DAC DMA 完成回调停止播放：

```c
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
    if (hdac->Instance == DAC1) {
        HAL_TIM_Base_Stop(&htim6);
        HAL_DAC_Stop_DMA(hdac, DAC_CHANNEL_1);
        HAL_DAC_SetValue(hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2048U);
        ttsBusy = false;
    }
}
```

如果 `ttsBusy` 定义在 `tts_player.c` 内部，则建议回调也放在该文件中，或提供内部函数由回调转发。

### 8) 应用层状态机

KEY0 开始录音推荐流程：

```text
KEY0 边沿触发
    ↓
若 Recorder 处于 IDLE 或 ERROR
    ↓
播放 “开始录音”
    ↓
进入 APP_WAIT_START_TTS_DONE
    ↓
检测 TTS_Player_IsBusy() == false
    ↓
Recorder_Init()
    ↓
Recorder_Start()
    ↓
LCD_DisplayRecordingStarted()
```

KEY1 停止录音推荐流程：

```text
KEY1 边沿触发
    ↓
若正在录音
    ↓
Recorder_Stop()
    ↓
LCD_DisplayRecordingStopped()
    ↓
播放 “录音已停止”
```

建议新增应用状态：

```c
typedef enum {
    APP_STATE_IDLE = 0,
    APP_STATE_WAIT_START_TTS_DONE,
    APP_STATE_RECORDING
} AppState_t;
```

也可以不新增完整应用状态机，只新增一个布尔标志，例如：

```c
static bool pendingStartRecordingAfterTts = false;
```

对于当前工程，布尔标志方案更小，状态机方案更清晰。

## Data Preparation

推荐 PC 侧处理流程：

1. 生成 TTS WAV 文件
2. 转成 16kHz 单声道 signed 16-bit PCM
3. 转成 12-bit DAC `uint16_t` 数组
4. 放入 `tts_audio_data.c`

示例 ffmpeg 命令：

```bash
ffmpeg -i start_record.wav -ac 1 -ar 16000 -f s16le start_record.pcm
```

数组生成脚本可以用 Python 完成：

```python
import struct

with open("start_record.pcm", "rb") as f:
    pcm = f.read()

values = []
for i in range(0, len(pcm), 2):
    sample = struct.unpack("<h", pcm[i:i+2])[0]
    dac = (sample + 32768) >> 4
    values.append(max(0, min(4095, dac)))
```

## Error Handling

- `HAL_DAC_Start_DMA()` 失败时清除 busy 并返回 `false`
- `HAL_TIM_Base_Start()` 失败时停止 DAC DMA，清除 busy 并返回 `false`
- `TTS_Player_Stop()` 应可在任何状态调用
- 播放中再次请求播放时返回 `false`，不打断当前提示音
- 播放结束后始终将 DAC 输出恢复到 `2048`

## Hardware Notes

推荐输出连接：

```text
PA4 / DAC1_OUT1 -> 1uF~10uF 隔直电容 -> 功放输入 -> 喇叭
STM32 GND -> 功放 GND
```

注意：

- `PA4` 不能直接驱动普通无源喇叭
- 如果使用 PAM8403，喇叭必须接模块输出端的 `L+ / L-` 或 `R+ / R-`，不要把喇叭负极接系统 GND
- 若声音破音，应在生成数组时降低波形幅度，避免长期接近 `0` 或 `4095`

## Verification Plan

1. **TIM6 频率验证**：确认 TIM6 更新频率为 16kHz
2. **DAC 静音验证**：未播放时 PA4 电压应在中间电平附近
3. **播放完成验证**：提示音播放一次后自动停止，不循环
4. **按键联调验证**：KEY0 播放提示音后再开始录音，KEY1 停止录音后播放提示音
5. **录音污染验证**：KEY0 的“开始录音”提示音不应出现在录音文件开头
6. **错误路径验证**：播放失败不会导致录音状态机卡死
