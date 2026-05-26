## Why

为项目添加 GPS 定位功能，使电子工牌能够实时获取和显示当前位置信息。ATGM336H GPS 模块支持多卫星系统（GPS/北斗/GLONASS/伽利略），通过串口与 STM32L431RCT6 通信，解析 NMEA 0183 协议中的 GNRMC 语句获取经纬度数据，并在 ST7735 LCD 屏幕上显示。

## What Changes

- 新增 GPS 模块驱动，支持 ATGM336H 定位模块
- 新增 USART2 串口接收（PA2-RX, PA3-TX），波特率 9600
- 解析 NMEA 0183 协议中的 GNRMC 语句（双模模式）
- 实现度分格式到十进制度格式的坐标转换（ddmm.mmmm → dd.dddddd）
- 在 LCD 屏幕上显示经纬度、时间、定位状态信息
- 支持定位数据有效性判断（A=有效，V=无效）

## Capabilities

- `gps-data-receive`: GPS 串口数据接收，使用 USART2 中断接收 ATGM336H 发送的 NMEA 数据
- `nmea-parser`: NMEA 0183 协议解析器，解析 GNRMC 语句提取时间、经纬度、方向等信息
- `gps-display`: GPS 数据显示，在 LCD 屏幕上显示位置信息

### Modified Capabilities

（无）

## Impact

- **新增文件**: 
  - `Core/Inc/gps.h` - GPS 模块头文件
  - `Core/Src/gps.c` - GPS 模块实现
- **修改文件**: 
  - `Core/Src/main.c` - 初始化 GPS 模块，在主循环中处理 GPS 数据
  - `Core/Inc/main.h` - 可能需要添加 GPS 相关的类型定义
- **硬件依赖**: 
  - USART2 (PA2-RX, PA3-TX)
  - ATGM336H GPS 模块
- **后续扩展**: 未来可能添加网络功能，将定位数据发送到服务器
