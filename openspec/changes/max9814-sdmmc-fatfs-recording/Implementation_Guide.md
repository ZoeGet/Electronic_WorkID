# MAX9814 + SDMMC + FATFS 录音实现文档

## 1. 模块划分

- `recorder.c/.h`：录音状态机与流程调度
- `audio_buffer.c/.h`：DMA 数据块转换、双缓冲状态管理
- `wav_format.c/.h`：WAV 头构建与回填

## 2. 关键接口设计

```c
typedef enum {
    REC_OK = 0,
    REC_ERR_PARAM,
    REC_ERR_SD,
    REC_ERR_FS,
    REC_ERR_BUSY,
    REC_ERR_STATE
} RecResult_t;

RecResult_t Recorder_Init(void);
RecResult_t Recorder_Start(const char *filename, uint32_t expected_bytes);
RecResult_t Recorder_Process(void);
RecResult_t Recorder_Stop(void);
```

## 3. 录音流程

### 3.1 初始化

1. 初始化外设（ADC/TIM2/SDMMC/FATFS）。
2. `f_mount` 挂载文件系统。
3. 初始化双缓冲状态（`ping_ready=0`, `pong_ready=0`）。

### 3.2 开始录音

1. 创建 WAV 文件：`f_open(filename, FA_CREATE_ALWAYS | FA_WRITE)`。
2. 写入 44 字节占位 WAV 头。
3. `f_lseek` 预分配（可选）。
4. 启动 TIM2，再启动 ADC DMA。

### 3.3 运行中写入

1. DMA half/full 回调仅置位“某块 ready”标志。
2. 主循环轮询 `Recorder_Process()`：
   - 检查 ready 块
   - 转换为 16-bit PCM（必要时做偏移校正）
   - 按 512B 对齐调用 `f_write`
   - 累计 `pcm_bytes_written`
3. 每 16KB 调用一次 `f_sync`。

### 3.4 停止录音

1. 停止 ADC DMA 和 TIM2。
2. `f_lseek(file, 0)` 回写 WAV 头：
   - `riff_size = 36 + pcm_bytes_written`
   - `data_size = pcm_bytes_written`
3. `f_sync` + `f_close`。

## 4. WAV 头字段（16k/16bit/mono）

- ChunkID: `"RIFF"`
- Format: `"WAVE"`
- Subchunk1ID: `"fmt "`
- AudioFormat: `1`（PCM）
- NumChannels: `1`
- SampleRate: `16000`
- BitsPerSample: `16`
- ByteRate: `16000 * 1 * 16 / 8 = 32000`
- BlockAlign: `1 * 16 / 8 = 2`
- Subchunk2ID: `"data"`

## 5. DMA 回调约束

- 回调中禁止执行 FATFS 写文件函数。
- 回调中只做：
  - 标志位更新
  - 必要的短路径计数
- 复杂逻辑统一在 `Recorder_Process()` 执行。

## 6. 可靠性建议

1. 写入异常后立即切换错误态，停止采样链路。
2. 增加录音期间心跳日志（可选 UART）。
3. 加入 overrun 计数，超过阈值后报警。
4. 录音文件命名建议带时间戳或自增序号，避免覆盖。

## 7. 最小验证步骤

1. 录音 10 秒并停止。
2. 在 PC 打开 WAV 文件检查属性（16kHz/16-bit/mono）。
3. 播放音频确认内容完整无明显断点。
4. 连续录音 60 秒观察是否出现 overrun。
