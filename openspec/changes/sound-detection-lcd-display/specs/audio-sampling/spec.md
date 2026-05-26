## ADDED Requirements

### Requirement: 音频采样配置
系统 SHALL 使用 ADC1_IN1(PC0) 引脚配合 TIM2 触发源实现 16kHz 的音频信号采样，采样数据通过 DMA 自动传输到内存缓冲区。

#### Scenario: 采样率精确度
- **WHEN** TIM2 配置为 PSC=0, ARR=4999，系统时钟 80MHz
- **THEN** ADC 采样率应为精确的 16kHz（误差<0.1%）

#### Scenario: DMA 连续传输
- **WHEN** DMA 配置为循环模式（Circular Mode）
- **THEN** ADC 数据应持续传输到缓冲区，无需 CPU 干预

#### Scenario: 缓冲区大小
- **WHEN** 缓冲区大小配置为 128 采样点
- **THEN** 应能存储 8ms 的音频数据（128 / 16000 = 0.008s）

### Requirement: 底噪动态跟踪
系统 SHALL 实时跟踪环境噪声水平，使用一阶低通滤波器动态更新底噪基准值，以适应环境变化。

#### Scenario: 底噪初始化
- **WHEN** 系统首次启动时
- **THEN** 底噪初始值应通过采样前 100ms（1600 点）的最小值确定

#### Scenario: 底噪更新算法
- **WHEN** 每个采样窗口（64 点）处理完成时
- **THEN** 底噪应按公式更新：`noise_floor = noise_floor * 0.98 + min_sample * 0.02`

#### Scenario: 底噪上限保护
- **WHEN** 计算出的底噪值超过 3000 ADC 值
- **THEN** 应将底噪限制在 3000，防止饱和失真导致的错误跟踪

### Requirement: 声音幅度计算
系统 SHALL 在每个采样窗口内计算信号的峰值 - 峰值（Peak-to-Peak），作为声音强度的衡量标准。

#### Scenario: 峰值检测
- **WHEN** 处理 64 点采样窗口时
- **THEN** 应找出窗口内的最大值（max_val）和最小值（min_val）

#### Scenario: 幅度计算
- **WHEN** 获得最大值和最小值后
- **THEN** 幅度值应为：`amplitude = max_val - min_val`

#### Scenario: 直流偏置处理
- **WHEN** 信号包含约 1.6V 直流偏置（ADC 值约 2048）
- **THEN** 峰值 - 峰值计算应自动消除直流偏置影响

### Requirement: 声音事件触发
系统 SHALL 基于幅度阈值判断是否发生声音事件，当检测到有效声音时触发后续动作。

#### Scenario: 阈值比较
- **WHEN** 计算出的幅度值超过 `noise_floor + DELTA_THRESHOLD`
- **THEN** 应判定为检测到声音事件

#### Scenario: 防抖锁定
- **WHEN** 一次声音事件被触发后
- **THEN** 应启动 500ms 的冷却期（cooldown），期间忽略其他触发

#### Scenario: 冷却期释放
- **WHEN** 500ms 冷却期结束后
- **THEN** 应解除锁定状态，允许下一次触发

#### Scenario: 阈值参数可调
- **WHEN** `DELTA_THRESHOLD` 参数在 150-300 范围内调整时
- **THEN** 系统应能相应改变触发灵敏度

### Requirement: LCD 声音提示显示
系统 SHALL 在检测到声音事件时切换 LCD 显示内容，显示"Sound Detected"英文提示。

#### Scenario: 触发切屏
- **WHEN** 声音事件被检测到时
- **THEN** LCD 应立即切换到声音提示页面

#### Scenario: 显示内容
- **WHEN** 显示声音提示页面时
- **THEN** 应显示文字"Sound Detected"，字体清晰可见

#### Scenario: 自动返回
- **WHEN** 声音提示显示 3 秒后
- **THEN** LCD 应自动恢复到原来的显示内容

#### Scenario: 连续触发处理
- **WHEN** 在显示提示期间再次检测到声音
- **THEN** 应重新计时 3 秒，保持显示状态

### Requirement: 系统初始化
系统 SHALL 在上电后正确初始化所有硬件模块，包括 ADC、DMA、定时器和 LCD。

#### Scenario: 硬件初始化顺序
- **WHEN** 系统上电启动时
- **THEN** 初始化顺序应为：时钟 → GPIO → ADC → DMA → TIM2 → LCD

#### Scenario: DMA 启动
- **WHEN** 所有初始化完成后
- **THEN** 应调用 `HAL_ADC_Start_DMA()` 启动连续采样

#### Scenario: 中断使能
- **WHEN** DMA 配置完成后
- **THEN** 应使能 DMA 半传输和全传输中断

### Requirement: 调试支持
系统 SHALL 提供串口调试输出功能，用于实时监控信号幅度和触发状态。

#### Scenario: 幅度串口打印
- **WHEN** 调试模式使能时（DEBUG_ENABLE 宏定义）
- **THEN** 应通过串口打印实时幅度值和底噪值

#### Scenario: 触发事件打印
- **WHEN** 检测到声音事件时
- **THEN** 应通过串口打印"Sound Detected!"消息
