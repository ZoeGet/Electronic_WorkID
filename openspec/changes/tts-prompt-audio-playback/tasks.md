## 1. CubeMX / 外设配置

- [ ] 1.1 确认 `DAC1 Channel 1` 启用，输出引脚为 `PA4 / DAC1_OUT1`
- [ ] 1.2 确认 `DAC1 Channel 1` 触发源为 `TIM6 TRGO`
- [ ] 1.3 确认 `TIM6` 的 `Master Output Trigger` 为 `Update Event`
- [ ] 1.4 配置 `TIM6` 为 16kHz 更新频率：80MHz 时钟下建议 `PSC=0, ARR=4999`
- [ ] 1.5 确认 DAC DMA 为 `Memory To Peripheral`
- [ ] 1.6 将 DAC DMA 模式设置为 `DMA_NORMAL`
- [ ] 1.7 确认 DAC DMA 数据宽度为 `Half Word / Half Word`
- [ ] 1.8 确认 DAC DMA Memory Increment 开启、Peripheral Increment 关闭

## 2. TTS 音频数据准备

- [ ] 2.1 确定需要的提示语列表，至少包括“开始录音”“录音已停止”
- [ ] 2.2 在 PC 上生成 TTS WAV 文件
- [ ] 2.3 将 WAV 转换为 16kHz、单声道、signed 16-bit PCM
- [ ] 2.4 将 PCM 转换为 12-bit DAC `uint16_t` 数组
- [ ] 2.5 新建或更新 `tts_audio_data.h/.c` 保存音频数组和长度
- [ ] 2.6 控制提示音时长，建议每条 0.3s~1.5s，避免占用过多 Flash
- [ ] 2.7 如声音破音，重新生成低幅度数组

## 3. 播放模块实现

- [ ] 3.1 新建 `Core/Inc/tts_player.h`
- [ ] 3.2 新建 `Core/Src/tts_player.c`
- [ ] 3.3 实现 `TTS_Player_Init()`，初始化 busy 状态并设置 DAC 静音中点
- [ ] 3.4 实现 `TTS_Player_IsBusy()` 查询播放状态
- [ ] 3.5 实现内部通用播放函数 `TTS_Player_Play(data, length)`
- [ ] 3.6 实现 `TTS_Player_PlayStartRecord()`
- [ ] 3.7 实现 `TTS_Player_PlayStopRecord()`
- [ ] 3.8 实现可选的 `TTS_Player_PlayStorageError()`
- [ ] 3.9 实现 `TTS_Player_Stop()`，支持强制停止播放并恢复静音中点
- [ ] 3.10 实现 DAC DMA 完成回调，播放结束后停止 TIM6 和 DAC DMA

## 4. main.c 集成

- [ ] 4.1 在 `main.c` 引入 `tts_player.h`
- [ ] 4.2 在外设初始化完成后调用 `TTS_Player_Init()`
- [ ] 4.3 在 KEY0 边沿触发处改为先播放“开始录音”
- [ ] 4.4 新增 `pendingStartRecordingAfterTts` 或应用状态机，等待提示音播放完成
- [ ] 4.5 在提示音播放完成后再执行 `Recorder_Init()` 和 `Recorder_Start()`
- [ ] 4.6 在 KEY1 成功 `Recorder_Stop()` 后播放“录音已停止”
- [ ] 4.7 在 SD/FATFS 错误显示时可选播放“存储错误”
- [ ] 4.8 确保播放期间不阻塞主循环

## 5. 构建系统接入

- [ ] 5.1 如果使用 CMake，确认 `tts_player.c` 被加入编译
- [ ] 5.2 如果使用 STM32CubeIDE 工程，确认新增 `.c/.h` 文件在工程目录中可见
- [ ] 5.3 编译工程，修复缺失 include、重复回调或未定义符号

## 6. 硬件接线

- [ ] 6.1 准备功放模块，例如 PAM8403、LM386 或有源音箱
- [ ] 6.2 `PA4 / DAC1_OUT1` 通过 1uF~10uF 隔直电容连接到功放输入
- [ ] 6.3 STM32 GND 与功放 GND 共地
- [ ] 6.4 喇叭连接到功放输出端，不要直接接 PA4
- [ ] 6.5 如果使用 PAM8403，喇叭负极不要接系统 GND

## 7. 联调验证

- [ ] 7.1 上电后未播放时，确认 DAC 输出处于中间电平附近
- [ ] 7.2 单独调用开始录音提示音，确认只播放一次且自动停止
- [ ] 7.3 按下 KEY0，确认先播放“开始录音”，播放完后开始录音
- [ ] 7.4 检查录音文件开头，确认没有录入“开始录音”提示音
- [ ] 7.5 按下 KEY1，确认先停止录音，再播放“录音已停止”
- [ ] 7.6 连续多次按键，确认不会重复打断播放或导致状态机卡死
- [ ] 7.7 触发 SD 错误路径，确认系统仍能显示错误并可恢复
