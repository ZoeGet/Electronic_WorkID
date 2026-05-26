## Why

项目需要在用户执行指定动作时播放语音提示，例如按下 KEY0 准备开始录音时播放“开始录音”，按下 KEY1 停止录音后播放“录音已停止”。当前系统已经具备录音、LCD、GPS 和 SD 卡写入能力，但缺少明确的语音反馈，用户只能通过屏幕判断状态，交互反馈不够直接。

本变更通过 `DAC1 + TIM6 + DMA` 播放预生成的 TTS 语音数组，实现非阻塞、低复杂度、无需额外 TTS 芯片的提示音功能。

## What Changes

- 新增 TTS 提示音播放能力：基于 `DAC1_OUT1(PA4) + TIM6 TRGO + DMA_NORMAL` 输出预生成语音数据
- 新增播放模块：建议新增 `tts_player.h/.c`，封装初始化、播放、停止、忙碌状态查询
- 新增 TTS 数据文件：建议新增 `tts_audio_data.h/.c`，保存 16kHz 单声道 12-bit DAC 音频数组
- 调整 DAC DMA 模式：从循环播放用途的 `DMA_CIRCULAR` 改为一次性提示音更合适的 `DMA_NORMAL`
- 调整应用流程：KEY0 播放“开始录音”完成后再启动录音，避免提示音被录进 WAV；KEY1 停止录音后播放“录音已停止”
- 新增播放完成回调：在 DAC DMA 完成后停止 TIM6、停止 DAC DMA，并将 DAC 输出恢复到静音中点

## Capabilities

- `tts-prompt-playback`: 固定 TTS 提示音的 DAC DMA 播放能力
- `button-action-audio-feedback`: 按键动作触发语音反馈
- `recording-safe-start-prompt`: 开始录音提示音播完后再启动录音，避免提示音写入录音文件
- `non-blocking-audio-output`: 播放期间主循环继续处理录音、GPS、LCD 与错误状态

## Impact

**硬件依赖**：
- `PA4 / DAC1_OUT1` 作为模拟音频输出
- `PA4` 后级需要连接隔直电容与音频功放模块，例如 PAM8403、LM386 或有源音箱输入
- 不建议直接用 `PA4` 推动无源喇叭

**软件依赖**：
- STM32 HAL：DAC、TIM、DMA
- 当前工程已有 `MX_DAC1_Init()`、`MX_TIM6_Init()`、`MX_DMA_Init()` 基础配置

**代码影响**：
- 新增 `Core/Inc/tts_player.h`
- 新增 `Core/Src/tts_player.c`
- 新增 `Core/Inc/tts_audio_data.h`
- 新增 `Core/Src/tts_audio_data.c`
- 更新 `Core/Src/dac.c` 中 DAC DMA 模式为 `DMA_NORMAL`
- 更新 `Core/Src/main.c` 的按键处理流程和初始化流程
- 如使用 CMake 构建，需要确认新增 `.c` 文件被纳入编译

**性能影响**：
- DMA 自动搬运音频数据，CPU 负担较小
- 16kHz、12-bit 单声道提示音每秒约 32KB Flash 数据，适合短语音提示
- 播放期间会占用 `DMA1_Channel3`、`DAC1` 和 `TIM6`

## Recommended Direction

采用以下固定播放配置：

- 音频采样率：`16000 Hz`
- 音频声道：单声道
- 音频数组类型：`const uint16_t[]`
- DAC 数据范围：`0 ~ 4095`
- DAC 静音中点：`2048`
- DAC 对齐：`DAC_ALIGN_12B_R`
- DAC 触发：`DAC_TRIGGER_T6_TRGO`
- TIM6：`PSC = 0`，`ARR = 4999`，当 TIM6 时钟为 80MHz 时输出 16kHz 更新事件
- DMA 模式：`DMA_NORMAL`
- 播放策略：非阻塞播放，播放中忽略新的播放请求
