## Why

实现基于声音触发的 LCD 自动切屏功能，当检测到打响指、喊叫等明显声音时，系统能够自动切换显示界面并显示"Sound Detected"提示。该功能利用 MAX9814 音频模块和 STM32 的 ADC+DMA+ 定时器硬件组合，实现低功耗、高精度的实时声音检测，无需持续占用 CPU 资源。

## What Changes

- 新增 ADC 采样功能：使用 ADC1_IN1(PC0) 配合 TIM2 触发，实现精确的 16kHz 采样率
- 新增 DMA 循环传输：自动将 ADC 采样数据存储到缓冲区，减少 CPU 干预
- 新增声音检测算法：基于信号幅度和底噪阈值的实时声音事件检测
- 新增 LCD 切屏显示：检测到声音事件时，切换显示界面并显示"Sound Detected"
- 新增音频信号处理：直流偏置消除、信号幅度计算、底噪补偿

## Capabilities

- `audio-sampling`: 基于 ADC+DMA+ 定时器的音频信号采集，16kHz 采样率，硬件触发
- `sound-detection`: 实时声音事件检测算法，包含底噪处理、阈值判断、防抖逻辑
- `lcd-sound-display`: LCD 显示声音检测结果，显示"Sound Detected"提示信息

### Modified Capabilities
- 无

## Impact

**硬件依赖**：
- MAX9814 音频放大模块（GAIN 悬空=50dB，AR 悬空=自动增益）
- ADC1_IN1 (PC0) 引脚连接
- TIM2 用于 ADC 触发
- DMA1_Channel1 用于数据传输
- LCD 显示屏（ST7735 驱动）

**软件依赖**：
- STM32 HAL 库（ADC、DMA、Timer）
- 已有的 ST7735 LCD 驱动（st7735.c/.h）
- 已有的字体库（fonts.c/.h）

**代码影响**：
- 需要修改 main.c 添加音频采样和声音检测逻辑
- 可能需要添加新的音频处理源文件（audio_detect.c/.h）
- LCD 显示逻辑需要支持声音事件触发切屏

**性能影响**：
- DMA 持续运行，占用少量总线带宽
- 声音检测算法在 DMA 传输完成回调中执行，占用少量 CPU 时间
- 内存占用：ADC 缓冲区（建议 256 采样点 × 2 字节 = 512 字节）

**用户体验**：
- 实现非接触式声音触发交互
- 需要合理设置底噪阈值以避免误触发
- 需要防抖逻辑避免重复触发
