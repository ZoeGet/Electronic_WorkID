# TTS 提示音功能：你需要做什么

本文档面向实际操作，告诉你在实现 `tts-prompt-audio-playback` 这个 OpenSpec 变更前后，需要准备什么、确认什么、测试什么。

对应 OpenSpec 变更目录：

```text
openspec/changes/tts-prompt-audio-playback/
```

## 1. 你需要准备的硬件

### 1.1 功放模块

`PA4 / DAC1_OUT1` 只是 STM32 的模拟电压输出，不能直接推动普通无源喇叭。

你需要准备一种音频放大方案：

- PAM8403 功放模块，推荐
- LM386 功放模块
- 有源小音箱
- 其他带音频输入的功放模块

### 1.2 推荐接线

```text
STM32 PA4 / DAC1_OUT1 -> 1uF~10uF 隔直电容 -> 功放音频输入
STM32 GND             -> 功放 GND
功放输出              -> 喇叭
```

如果你使用 PAM8403：

```text
喇叭接 PAM8403 的 L+ / L- 或 R+ / R-
不要把喇叭负极接 STM32 GND
```

### 1.3 供电注意

- STM32 和功放必须共地
- 功放供电应满足模块要求，例如 PAM8403 常见为 5V
- 如果播放时杂音明显，优先检查供电、共地、线长和功放输入连接

## 2. 你需要准备的语音文件

至少准备两条 TTS：

| 文件含义 | 建议文字 |
|---|---|
| 开始录音提示 | 开始录音 |
| 停止录音提示 | 录音已停止 |

可选再准备：

| 文件含义 | 建议文字 |
|---|---|
| 存储错误提示 | 存储错误 |
| 请检查 SD 卡 | 请检查存储卡 |

## 3. 你需要把 TTS 转成什么格式

最终要转成 C 语言数组，格式如下：

```c
const uint16_t tts_start_record[] = {
    2048, 2050, 2055
};
```

推荐音频参数：

| 参数 | 值 |
|---|---|
| 采样率 | 16000 Hz |
| 声道 | 单声道 |
| 数据类型 | uint16_t |
| DAC 范围 | 0 ~ 4095 |
| 静音中点 | 2048 |

## 4. 如何转换语音文件

### 4.1 先生成 WAV

你可以用任意 TTS 工具生成 WAV，例如：

- 在线 TTS 网站
- edge-tts
- 讯飞、Azure、其他语音合成工具

生成后，建议文件命名为：

```text
start_record.wav
stop_record.wav
storage_error.wav
```

### 4.2 用 ffmpeg 转 PCM

如果你电脑安装了 ffmpeg，可以执行：

```bash
ffmpeg -i start_record.wav -ac 1 -ar 16000 -f s16le start_record.pcm
ffmpeg -i stop_record.wav -ac 1 -ar 16000 -f s16le stop_record.pcm
ffmpeg -i storage_error.wav -ac 1 -ar 16000 -f s16le storage_error.pcm
```

### 4.3 用脚本转 C 数组

转换公式是：

```text
signed 16-bit PCM: -32768 ~ 32767
DAC 12-bit:        0 ~ 4095

DAC = (PCM + 32768) >> 4
```

如果声音太大、破音，需要降低幅度。推荐让输出大约落在：

```text
500 ~ 3600
```

不要长期贴近：

```text
0 或 4095
```

## 5. 你需要确认的 CubeMX 配置

### 5.1 DAC1

确认：

```text
DAC1 Channel 1 Enable
Output: PA4 / DAC1_OUT1
Trigger: TIM6 TRGO
Output Buffer: Enable
DMA: Enable
```

### 5.2 TIM6

目标是 16kHz 更新事件。

如果 TIM6 时钟是 80MHz，建议：

```text
Prescaler = 0
Counter Period = 4999
Master Output Trigger = Update Event
```

计算关系：

```text
80,000,000 / (0 + 1) / (4999 + 1) = 16000 Hz
```

### 5.3 DMA

DAC DMA 建议：

```text
Direction: Memory To Peripheral
Mode: Normal
Peripheral Increment: Disable
Memory Increment: Enable
Peripheral Data Width: Half Word
Memory Data Width: Half Word
Priority: Low 或 Medium
```

重点是：

```text
Mode 必须是 Normal，不建议 Circular
```

否则提示音可能会循环播放。

## 6. 你需要确认的代码改动方向

实现时会新增或修改这些文件：

```text
Core/Inc/tts_player.h
Core/Src/tts_player.c
Core/Inc/tts_audio_data.h
Core/Src/tts_audio_data.c
Core/Src/dac.c
Core/Src/main.c
```

如果使用 CMake，还需要确认新增 `.c` 文件被纳入编译。

## 7. 按键逻辑应该如何变化

### 7.1 KEY0 开始录音

最终逻辑应该是：

```text
按下 KEY0
    ↓
播放 “开始录音”
    ↓
等待播放完成
    ↓
初始化录音模块
    ↓
开始录音
    ↓
LCD 显示 Recording Started
```

这样做的原因是：

```text
避免“开始录音”这句话被麦克风录进 WAV 文件。
```

### 7.2 KEY1 停止录音

最终逻辑应该是：

```text
按下 KEY1
    ↓
停止录音
    ↓
回填 WAV 文件头
    ↓
LCD 显示 Recording Stopped
    ↓
播放 “录音已停止”
```

## 8. 你需要怎么测试

### 8.1 单独测试播放

先不要关心录音，只测试能不能播放一条提示音：

- 上电后调用一次 `TTS_Player_PlayStartRecord()`
- 听喇叭是否有声音
- 确认只播放一次，不循环

### 8.2 测试 KEY0

按下 KEY0 后检查：

- 是否先听到“开始录音”
- 是否语音结束后才开始录音
- LCD 是否显示录音开始
- 录音文件开头是否没有“开始录音”这句话

### 8.3 测试 KEY1

录音中按下 KEY1 后检查：

- 是否先停止录音
- WAV 文件是否可播放
- 是否听到“录音已停止”

### 8.4 测试异常情况

建议测试：

- 连续快速按 KEY0 / KEY1
- 播放中再次按键
- SD 卡异常时是否还能显示错误
- 提示音是否导致系统卡死

## 9. 常见问题排查

### 9.1 完全没声音

检查顺序：

1. PA4 是否真的连接到功放输入
2. STM32 和功放是否共地
3. 功放是否供电正常
4. DAC 是否启用 Channel 1
5. TIM6 是否启动
6. DAC DMA 是否启动成功
7. 音频数组长度是否大于 0

### 9.2 声音一直循环

大概率是 DMA 配成了：

```text
DMA_CIRCULAR
```

应改成：

```text
DMA_NORMAL
```

### 9.3 声音很小

可能原因：

- 没有功放
- 功放增益太低
- 音频数组幅度太小
- 隔直电容或输入连接不合适

### 9.4 声音破音

可能原因：

- 音频数组幅度太大
- DAC 数据频繁接近 0 或 4095
- 功放输入过载
- 供电噪声大

解决方法：

- 降低数组生成时的音量
- 控制 DAC 值大致在 500~3600
- 降低功放音量

### 9.5 开始录音提示音被录进文件

说明 KEY0 流程不对。

应改成：

```text
先播放提示音，播放完成后再 Recorder_Start()
```

## 10. 推荐实施顺序

建议你按这个顺序推进：

1. 准备功放和喇叭，确认 PA4 能接到功放输入
2. 生成“开始录音”和“录音已停止”两条 TTS WAV
3. 转成 16kHz 单声道 PCM
4. 转成 `uint16_t` DAC 数组
5. 实现 `tts_audio_data.h/.c`
6. 实现 `tts_player.h/.c`
7. 把 DAC DMA 改成 `DMA_NORMAL`
8. 单独测试播放
9. 集成 KEY0，先播放再录音
10. 集成 KEY1，停止后播放
11. 做连续按键和录音文件验证
