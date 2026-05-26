## ADDED Requirements

### Requirement: GPS 串口数据接收
系统应通过 USART2 串口接收 ATGM336H GPS 模块发送的 NMEA 0183 数据

#### Scenario: 上电初始化 USART2
- **WHEN** 系统上电启动
- **THEN** USART2 应被正确初始化，波特率为 9600，8 位数据位，1 位停止位，无校验

#### Scenario: 配置 GPIO 引脚
- **WHEN** 初始化 USART2 时
- **THEN** PA2 应配置为 USART2_RX（复用功能），PA3 应配置为 USART2_TX（复用功能）

#### Scenario: 使能接收中断
- **WHEN** USART2 初始化完成后
- **THEN** 应使能 USART2 接收中断（RXNE），以便实时接收 GPS 数据

#### Scenario: 接收 NMEA 数据
- **WHEN** GPS 模块通过串口发送 NMEA 0183 数据
- **THEN** USART2 中断应能正确接收每个字节并存储到接收缓冲区

#### Scenario: 检测语句起始符
- **WHEN** 接收到 `$` 字符时
- **THEN** 应清空接收缓冲区，并将接收指针重置为 0，准备接收新语句

#### Scenario: 缓冲区溢出保护
- **WHEN** 接收数据达到缓冲区最大长度（200 字节）时
- **THEN** 应停止接收，防止缓冲区溢出
