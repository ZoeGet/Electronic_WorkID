## ADDED Requirements

### Requirement: GNRMC 语句识别
系统应能识别并验证 GNRMC 语句的完整性

#### Scenario: 识别 GNRMC 标识
- **WHEN** 接收缓冲区前 5 个字符为 `$GNRMC` 时
- **THEN** 应确认收到有效的 GNRMC 语句，继续接收后续数据

#### Scenario: 检测语句结束符
- **WHEN** 接收到换行符 `\n` 时
- **THEN** 应确认 GNRMC 语句接收完成，触发解析流程

#### Scenario: 校验语句结构
- **WHEN** GNRMC 语句接收完成后
- **THEN** 应验证语句包含至少 6 个逗号分隔的字段

### Requirement: 时间信息提取
系统应从 GNRMC 语句中提取 UTC 时间信息

#### Scenario: 解析 UTC 时间字段
- **WHEN** GNRMC 语句解析时
- **THEN** 应提取第 1 个字段（UTC 时间，格式 hhmmss.sss）并存储到 UTCTime 缓冲区

#### Scenario: 时间格式验证
- **WHEN** 提取 UTC 时间后
- **THEN** 时间字符串长度应为 10 个字符（包括小数点）

### Requirement: 定位状态判断
系统应能判断 GPS 定位数据是否有效

#### Scenario: 解析定位状态字段
- **WHEN** GNRMC 语句解析时
- **THEN** 应提取第 2 个字段（定位状态，A=有效，V=无效）

#### Scenario: 有效定位标记
- **WHEN** 定位状态字段为 `A` 时
- **THEN** isUsefull 标志应设置为 true，表示定位数据有效

#### Scenario: 无效定位标记
- **WHEN** 定位状态字段为 `V` 时
- **THEN** isUsefull 标志应设置为 false，表示定位数据无效

### Requirement: 纬度信息提取
系统应从 GNRMC 语句中提取纬度信息

#### Scenario: 解析纬度字段
- **WHEN** GNRMC 语句解析时
- **THEN** 应提取第 3 个字段（纬度，格式 ddmm.mmmm）并存储到 latitude 缓冲区

#### Scenario: 解析纬度方向
- **WHEN** GNRMC 语句解析时
- **THEN** 应提取第 4 个字段（纬度方向，N=北纬，S=南纬）并存储到 N_S 缓冲区

#### Scenario: 纬度数据验证
- **WHEN** 提取纬度信息后
- **THEN** 纬度字符串长度应为 9-10 个字符（包括小数点）

### Requirement: 经度信息提取
系统应从 GNRMC 语句中提取经度信息

#### Scenario: 解析经度字段
- **WHEN** GNRMC 语句解析时
- **THEN** 应提取第 5 个字段（经度，格式 dddmm.mmmm）并存储到 longitude 缓冲区

#### Scenario: 解析经度方向
- **WHEN** GNRMC 语句解析时
- **THEN** 应提取第 6 个字段（经度方向，E=东经，W=西经）并存储到 E_W 缓冲区

#### Scenario: 经度数据验证
- **WHEN** 提取经度信息后
- **THEN** 经度字符串长度应为 10-11 个字符（包括小数点）

### Requirement: 坐标格式转换
系统应将度分格式的坐标转换为十进制度格式

#### Scenario: 纬度转换计算
- **WHEN** 解析到纬度数据 2236.9453 时
- **THEN** 应计算 22 + (36.9453 / 60) = 22.615755

#### Scenario: 经度转换计算
- **WHEN** 解析到经度数据 11408.4790 时
- **THEN** 应计算 114 + (08.4790 / 60) = 114.141317

#### Scenario: 转换精度保证
- **WHEN** 进行坐标转换时
- **THEN** 转换结果应保留至少 6 位小数

#### Scenario: 负坐标处理
- **WHEN** 坐标为南纬或西经时
- **THEN** 转换结果应保持正确的符号（南纬为负，西经为负）

### Requirement: 解析错误处理
系统应能处理解析过程中的各种错误情况

#### Scenario: 字段缺失检测
- **WHEN** GNRMC 语句字段数量不足时
- **THEN** 应标记为解析错误，清空缓冲区，等待下一帧数据

#### Scenario: 数据格式错误
- **WHEN** 字段数据格式不符合预期时（如非数字字符）
- **THEN** 应标记为解析错误，不使用该帧数据

#### Scenario: 解析失败恢复
- **WHEN** 解析失败时
- **THEN** 应清空所有标志位和缓冲区，重新开始接收
