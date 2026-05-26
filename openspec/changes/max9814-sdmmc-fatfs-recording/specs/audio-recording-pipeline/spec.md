## ADDED Requirements

### Requirement: 16kHz 定时触发采样
系统 SHALL 使用 `TIM2 TRGO` 触发 `ADC1_IN1(PC0)`，并以 16kHz 进行稳定采样。

#### Scenario: 采样频率正确
- **WHEN** TIM2 时钟与参数满足 16kHz 计算关系
- **THEN** ADC 采样频率应保持在目标值附近（允许小误差）

#### Scenario: 采样连续
- **WHEN** 系统处于录音状态
- **THEN** ADC DMA 应持续运行，不应出现采样中断

### Requirement: 双缓冲不丢帧
系统 SHALL 使用双缓冲机制，使采样与写卡并行执行，避免录音中断和丢帧。

#### Scenario: Half/Full 回调协同
- **WHEN** DMA 半传输与全传输中断到来
- **THEN** 系统应分别标记对应缓冲区为可写入状态

#### Scenario: 写入阻塞保护
- **WHEN** 主循环写卡速度短时低于采样速度
- **THEN** 系统应记录 overrun 事件并上报状态

### Requirement: WAV 文件格式有效
系统 SHALL 将采样数据写入标准 WAV 文件，并在停止录音时修正头字段。

#### Scenario: 文件创建
- **WHEN** 用户开始录音
- **THEN** 系统应创建新 WAV 文件并写入 44 字节占位头

#### Scenario: 头回填
- **WHEN** 用户停止录音
- **THEN** 系统应回填 `RIFF chunk size` 与 `data chunk size`，使文件可被 PC 播放器识别

### Requirement: SD 写入优化
系统 SHALL 使用 FATFS 的批量写入与预分配机制保证连续录音性能。

#### Scenario: 扇区对齐写入
- **WHEN** 写入 PCM 数据
- **THEN** 每次写入大小应优先选择 512B 或其整数倍

#### Scenario: 预分配
- **WHEN** 开始录音时具备预计时长
- **THEN** 系统应执行 `f_lseek` 进行预分配并回到数据起点，减少碎片化

### Requirement: 可诊断错误处理
系统 SHALL 在 SD/FATFS 操作失败时进入错误状态并返回可诊断结果。

#### Scenario: FATFS 写入失败
- **WHEN** `f_write` 返回非 `FR_OK` 或写入长度不足
- **THEN** 系统应停止录音流程并报告写入失败

#### Scenario: 空间不足或介质异常
- **WHEN** 发生磁盘满、卡拔出或挂载失败
- **THEN** 系统应进入错误态并允许后续重新初始化恢复
