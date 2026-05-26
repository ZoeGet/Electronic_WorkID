## Context

目标 MCU 为 `STM32L431RCT6`，采样率固定为 `16kHz`。录音路径为：

`MAX9814 -> ADC1_IN1(PC0) -> DMA -> 双缓冲 -> FATFS -> SDMMC1 -> SD卡`

系统必须保证持续录音时不丢样点，并将数据以标准 WAV 格式保存，确保 PC 可直接播放。

## Goals / Non-Goals

**Goals**
- 实现 16kHz、16bit、单声道 WAV 录音
- 保证采样与写卡并行，连续录音不丢帧
- 提供可复用录音状态机接口（Init/Start/Process/Stop）
- 录音文件在异常停止后仍可识别（最小化损坏）

**Non-Goals**
- 不做语音编码压缩（如 ADPCM/MP3）
- 不做 VAD、降噪、回声消除等高级音频处理
- 不做多文件索引或播放器 UI

## Architecture

### 1) 采样时基

- 使用 `TIM2 TRGO` 触发 ADC，保证采样节拍稳定。
- 采样频率公式：`Fs = TIM2_CLK / (PSC+1) / (ARR+1)`。
- 示例（TIM2 时钟 80MHz）：`PSC=0, ARR=4999`，得到 `Fs=16kHz`。

### 2) ADC + DMA 双缓冲

- ADC 连续由 TIM2 触发，DMA 循环模式。
- DMA 半传输回调处理 buffer 前半段，全传输回调处理后半段。
- 回调中仅置位“待写标志”，避免在中断里执行 `f_write`。

### 3) 任务调度模型

- 主循环调用 `Recorder_Process()`：
  1. 检查是否有满块 PCM 数据
  2. 将数据按 512 字节对齐批量写入文件
  3. 维护累计字节数与错误状态

### 4) WAV 写入策略

- 录音开始：先写 44 字节占位 WAV 头。
- 录音中：持续写 `PCM` 数据块。
- 录音停止：回到文件起始位置回填 `RIFF size` 与 `data size`，再关闭文件。

### 5) 写入性能策略

- 批量写入：每次写入 512B（或其整数倍）。
- 预分配空间：`f_lseek(file,预估大小)` 后再 `f_lseek(file,44)`，减少碎片与扩展开销。
- 定期 `f_sync`：例如每 16KB 一次，平衡可靠性与速度。

## Core Data Structures

```c
typedef enum {
    REC_IDLE = 0,
    REC_READY,
    REC_RECORDING,
    REC_STOPPING,
    REC_ERROR
} RecorderState_t;

typedef struct {
    uint16_t adc_dma_buf[1024];   // DMA循环缓冲，示例
    uint8_t  pcm_ping[1024];      // 写卡缓冲A
    uint8_t  pcm_pong[1024];      // 写卡缓冲B
    volatile uint8_t ping_ready;
    volatile uint8_t pong_ready;
    volatile uint8_t overrun;
    FIL file;
    uint32_t pcm_bytes_written;
    RecorderState_t state;
} Recorder_t;
```

## Error Handling

- `f_mount/f_open/f_write/f_lseek/f_sync/f_close` 返回值非 `FR_OK` 时进入 `REC_ERROR`
- SD 卡拔出、写保护、空间不足均需返回可诊断错误码
- 发生错误后立即停止 ADC DMA，关闭文件并上报状态

## Verification Plan

1. **时基验证**：逻辑分析仪或调试日志确认采样率 16kHz。
2. **连续性验证**：持续录音 60s，检查无 DMA overrun 标志。
3. **文件验证**：PC 打开 WAV，采样率/位深/时长与预期一致。
4. **压力验证**：同时执行 LCD 刷屏或串口日志，确认不丢帧。
5. **异常验证**：录音中断电/拔卡，验证错误处理路径可恢复。
