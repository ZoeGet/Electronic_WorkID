## ADDED Requirements

### Requirement: 底噪动态跟踪算法
系统 SHALL 实现自适应底噪跟踪算法，使用一阶 IIR 低通滤波器实时估计环境噪声水平，确保在不同环境下的触发一致性。

#### Scenario: 初始底噪采集
- **WHEN** 系统启动后的前 100ms 内
- **THEN** 应采集至少 1600 个 ADC 样本，并记录最小值作为初始底噪

#### Scenario: 滑动窗口最小值检测
- **WHEN** 每 64 个采样点处理完成时
- **THEN** 应在该窗口内找出最小 ADC 值 `min_sample`

#### Scenario: 一阶低通滤波
- **WHEN** 获得当前窗口最小值后
- **THEN** 应使用公式 `noise_floor = noise_floor * 0.95 + min_sample * 0.05` 更新底噪

#### Scenario: 底噪变化率限制
- **WHEN** 单次更新的底噪变化量超过 50 ADC 值
- **THEN** 应限制变化量不超过 50，防止突变干扰

#### Scenario: 底噪上限保护
- **WHEN** 计算出的底噪值超过 3500 ADC 值
- **THEN** 应将底噪钳制在 3500，防止异常值

### Requirement: 信号幅度峰值检测
系统 SHALL 在每个采样窗口内计算信号的峰值 - 峰值（Peak-to-Peak），作为声音强度的直接度量。

#### Scenario: 窗口最大值检测
- **WHEN** 处理 64 点采样窗口时
- **THEN** 应遍历所有样本，找出最大值 `max_val`

#### Scenario: 窗口最小值检测
- **WHEN** 处理 64 点采样窗口时
- **THEN** 应遍历所有样本，找出最小值 `min_val`

#### Scenario: 峰值 - 峰值计算
- **WHEN** 获得 `max_val` 和 `min_val` 后
- **THEN** 幅度值应计算为 `amplitude = max_val - min_val`

#### Scenario: 直流偏置验证
- **WHEN** 输入信号为纯直流（无声音）时
- **THEN** 幅度值应接近 0（< 10 ADC 值）

#### Scenario: 交流信号响应
- **WHEN** 输入信号为 1kHz 正弦波，峰峰值 1V 时
- **THEN** 幅度值应与信号强度成正比

### Requirement: 触发阈值比较
系统 SHALL 将实时计算的幅度值与动态阈值进行比较，判断是否发生有效声音事件。

#### Scenario: 动态阈值计算
- **WHEN** 底噪值为 `noise_floor` 时
- **THEN** 触发阈值应为 `threshold = noise_floor + DELTA_THRESHOLD`

#### Scenario: 默认阈值参数
- **WHEN** 系统使用默认参数时
- **THEN** `DELTA_THRESHOLD` 应为 200 ADC 值

#### Scenario: 阈值比较逻辑
- **WHEN** `amplitude > threshold` 时
- **THEN** 应判定为检测到潜在声音事件

#### Scenario: 最小幅度保护
- **WHEN** `amplitude < 50` 时
- **THEN** 即使超过阈值也不触发，防止底噪波动误触发

### Requirement: 防抖锁定机制
系统 SHALL 实现触发锁定和冷却期机制，防止单次声音事件导致连续多次触发。

#### Scenario: 触发锁定
- **WHEN** 一次声音事件被确认触发时
- **THEN** 应设置 `trigger_locked = true` 标志

#### Scenario: 冷却期启动
- **WHEN** 触发锁定生效时
- **THEN** 应启动 500ms 的冷却定时器

#### Scenario: 冷却期忽略
- **WHEN** 冷却期内再次检测到声音时
- **THEN** 应忽略该声音事件，不触发新的动作

#### Scenario: 锁定释放
- **WHEN** 500ms 冷却期结束后
- **THEN** 应设置 `trigger_locked = false`，允许下一次触发

#### Scenario: 冷却时间可调
- **WHEN** `COOLDOWN_MS` 参数在 200-1000ms 范围内调整时
- **THEN** 系统应能相应改变防抖时间

### Requirement: 多级触发判断（可选增强）
系统 SHOULD 支持多级声音强度判断，区分轻敲、重击等不同强度的声音事件。

#### Scenario: 两级阈值配置
- **WHEN** 启用多级触发功能时
- **THEN** 应配置 `THRESHOLD_LOW` 和 `THRESHOLD_HIGH` 两个阈值

#### Scenario: 轻度声音检测
- **WHEN** `amplitude > THRESHOLD_LOW` 且 `amplitude <= THRESHOLD_HIGH` 时
- **THEN** 应判定为轻度声音事件（如轻敲）

#### Scenario: 重度声音检测
- **WHEN** `amplitude > THRESHOLD_HIGH` 时
- **THEN** 应判定为重度声音事件（如响指、喊叫）

#### Scenario: 多级响应
- **WHEN** 检测到不同级别的声音事件时
- **THEN** 可执行不同的响应动作（如不同显示内容）
