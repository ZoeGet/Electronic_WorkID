# STM32L431RCT6 + MAX9814 录音系统 CubeMX 配置指南

本指南对应硬件连接：
- MAX9814 OUT -> `PC0 (ADC1_IN1)`
- SD 卡：`PD2(CMD)`、`PC12(CK)`、`PC8(D0)`（1-bit）
- 目标采样率：`16kHz`

## 1. 时钟与基础

1. 在 `Clock Configuration` 中确认系统主频（常用 80MHz）。
2. 若使用 80MHz，后续 TIM2 参数可用 `PSC=0, ARR=4999` 直接得到 16kHz。
3. 使能 DMA 控制器时钟（CubeMX 生成时通常自动完成）。

## 2. ADC1 配置（MAX9814）

1. `PC0` 设置为 `ADC1_IN1`。
2. `ADC1 -> Parameter Settings`：
   - Resolution: `12-bit`
   - Data Alignment: `Right`
   - Scan Conv Mode: `Disable`
   - Continuous Conv Mode: `Disable`（由外部触发驱动）
   - External Trigger Conversion Source: `TIM2 TRGO`
   - External Trigger Conversion Edge: `Rising`
   - DMA Continuous Requests: `Enable`
   - Overrun: `Data overwritten`（推荐）
3. 通道采样时间建议先用 `47.5 cycles` 或更高，根据噪声再调。

## 3. TIM2 配置（16kHz 触发）

1. `TIM2 -> Clock Source`: `Internal Clock`
2. `TIM2 -> Counter Settings`：
   - Prescaler: `0`
   - Counter Period (ARR): `4999`
   - Counter Mode: `Up`
3. `TIM2 -> Trigger Output (TRGO)` 设为 `Update Event`。
4. 启动流程中调用 `HAL_TIM_Base_Start(&htim2)`。

## 4. ADC DMA 配置

1. 在 `ADC1 -> DMA Settings` 添加 DMA 请求：
   - Direction: `Peripheral to Memory`
   - Mode: `Circular`
   - Peripheral/Data Width: `Half Word`
   - Priority: `Very High`（推荐）
2. 在 NVIC 中开启对应 DMA 中断。
3. 代码中使用：
   - `HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buf, ADC_DMA_LEN);`
4. 实现 `HAL_ADC_ConvHalfCpltCallback` 与 `HAL_ADC_ConvCpltCallback`。

## 5. SDMMC1（1-bit）配置

1. 使能 `SDMMC1`，选择 1-bit 总线。
2. 引脚确认：
   - `PD2` -> `SDMMC1_CMD`
   - `PC12` -> `SDMMC1_CK`
   - `PC8` -> `SDMMC1_D0`
3. `SDMMC1 -> Parameter Settings`：
   - Bus Wide: `1B`
   - Hardware Flow Control: `Disable`（先保证稳定，再按实测开启）
   - Clock Power Save: `Disable`
   - Clock Div: 从保守值起步（如 8~10），稳定后再提速
4. `SDMMC1 -> DMA Settings`：
   - Rx: Peripheral to Memory
   - Tx: Memory to Peripheral
   - Mode: Normal
   - Priority: High
5. 使能 `SDMMC1 global interrupt`。

## 6. FATFS 配置

1. 在 `Middleware` 启用 `FATFS`。
2. Interface 选择 `SD Card`（绑定 SDMMC1 驱动）。
3. 推荐打开：
   - `Use DMA template`（若版本提供）
   - `Re-entrant` 仅在 RTOS 多任务访问文件系统时启用
4. 代码中上电后执行 `f_mount` 并检查返回值。

## 7. 录音相关代码启动顺序（建议）

1. `MX_GPIO_Init()`
2. `MX_DMA_Init()`
3. `MX_ADC1_Init()`
4. `MX_TIM2_Init()`
5. `MX_SDMMC1_SD_Init()`
6. `MX_FATFS_Init()`
7. `f_mount(...)`
8. 写 WAV 头占位，启动 `TIM2` 与 `ADC DMA`

## 8. 关键参数建议（首版）

- 采样格式：16kHz / 16-bit / Mono
- ADC DMA 环形缓冲：`1024 samples`
- 写卡 Ping-Pong 缓冲：每块 `1024 bytes`
- FATFS 写块：`512 bytes` 对齐
- `f_sync` 周期：每写入 `16KB` 执行一次

## 9. 常见问题排查

1. **录音断续/爆音**：
   - 先降 SDMMC 时钟
   - 检查是否在中断中直接 `f_write`
   - 增大写卡缓冲区
2. **WAV 无法播放**：
   - 检查停止录音时是否回填 WAV 头
   - 检查 `data size` 与实际 PCM 长度一致性
3. **采样率不对**：
   - 核对系统时钟与 TIM2 时钟源
   - 复算 `PSC/ARR`
