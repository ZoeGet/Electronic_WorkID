## ADDED Requirements

### Requirement: GPS 数据显示
系统应在 LCD 屏幕上显示 GPS 定位信息

#### Scenario: 显示定位状态
- **WHEN** 定位数据有效（isUsefull = true）时
- **THEN** 应在屏幕顶部显示 "GPS: OK" 或类似成功标识

#### Scenario: 显示无效定位
- **WHEN** 定位数据无效（isUsefull = false）时
- **THEN** 应显示 "GPS: NO FIX" 或 "搜索中..." 提示

#### Scenario: 显示纬度信息
- **WHEN** 定位数据有效时
- **THEN** 应显示纬度信息，格式为 "LAT: 22.615755 N"

#### Scenario: 显示经度信息
- **WHEN** 定位数据有效时
- **THEN** 应显示经度信息，格式为 "LON: 114.141317 E"

#### Scenario: 显示 UTC 时间
- **WHEN** 定位数据有效时
- **THEN** 应显示 UTC 时间，格式为 "UTC: 08:48:52"

### Requirement: 显示布局
GPS 信息应在 LCD 屏幕上合理布局

#### Scenario: 屏幕清空
- **WHEN** 更新 GPS 显示前
- **THEN** 应先清空屏幕或清除原显示区域

#### Scenario: 信息排列
- **WHEN** 显示多项信息时
- **THEN** 应按顺序垂直排列，每项信息占据一行

#### Scenario: 字体选择
- **WHEN** 显示 GPS 信息时
- **THEN** 应使用合适的字体大小，确保信息清晰可读

#### Scenario: 颜色搭配
- **WHEN** 显示文本时
- **THEN** 应使用对比度合适的颜色（如白底黑字或黑底白字）

### Requirement: 显示更新策略
系统应合理控制 GPS 信息的更新频率

#### Scenario: 数据更新触发
- **WHEN** 接收到新的有效 GNRMC 数据时
- **THEN** 应更新 LCD 显示内容

#### Scenario: 避免闪烁
- **WHEN** 频繁更新显示时
- **THEN** 应采用双缓冲或其他技术避免屏幕闪烁

#### Scenario: 无数据时显示
- **WHEN** 长时间未收到有效 GPS 数据时
- **THEN** 应显示 "等待 GPS 信号..." 或类似提示

### Requirement: 与现有功能集成
GPS 显示功能应与现有系统功能协调工作

#### Scenario: 不影响声音检测
- **WHEN** GPS 功能运行时
- **THEN** 不应影响现有的声音检测功能

#### Scenario: LED 状态指示
- **WHEN** GPS 定位成功时
- **THEN** 可通过 LED 指示定位状态（可选）

#### Scenario: 主循环集成
- **WHEN** 主循环运行时
- **THEN** GPS 数据处理和显示应与现有业务并行执行，互不阻塞
