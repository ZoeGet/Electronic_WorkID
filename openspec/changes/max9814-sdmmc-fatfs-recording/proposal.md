## Why

项目需要将 MAX9814 麦克风采样数据稳定保存为 SD 卡中的 WAV 文件，用于后续离线分析和回放。当前系统虽已具备 ADC 采样基础，但尚未形成完整的“采样-缓存-文件写入-文件收尾”闭环，无法满足连续录音场景。

## What Changes

- 新增录音能力：基于 `ADC1 + TIM2 + DMA + 双缓冲` 采集 16kHz PCM 数据
- 新增存储能力：基于 `SDMMC1(1-bit) + FATFS` 将 PCM 数据按 WAV 文件格式写入 SD 卡
- 新增缓冲调度：使用 Ping-Pong 双缓冲机制，采样与写卡并行，避免丢帧
- 新增文件生命周期：支持录音开始、运行中批量写入、停止时回填 WAV 头
- 新增可靠性机制：扇区对齐写入、写入错误处理、空间预分配策略

## Capabilities

- `audio-recording-pipeline`: MAX9814 到 WAV 文件的完整数据链路
- `wav-file-writer`: WAV 头创建、数据区持续写入、停止时头信息修正
- `dma-double-buffering`: ADC DMA 半/满回调驱动的双缓冲数据搬运
- `sd-storage-throughput`: 512 字节批量写入与预分配提升写入稳定性

## Impact

**硬件依赖**：
- MAX9814 OUT -> `PC0(ADC1_IN1)`
- SD 卡 1-bit：`PD2(CMD)`、`PC12(CK)`、`PC8(D0)`

**软件依赖**：
- STM32 HAL：ADC、TIM、DMA、SDMMC、FATFS
- FatFs：`f_mount/f_open/f_write/f_lseek/f_sync/f_close`

**代码影响**：
- 新增录音模块（建议 `recorder.c/.h`、`wav_format.c/.h`、`audio_buffer.c/.h`）
- 更新 `main.c`：初始化顺序、录音控制逻辑、中断回调转发

**性能影响**：
- CPU 负载主要在缓冲区状态管理与文件写入调度，采样搬运由 DMA 承担
- 典型吞吐需求仅约 `32KB/s`（16kHz, 16bit, 单声道），远低于 SDMMC 带宽
