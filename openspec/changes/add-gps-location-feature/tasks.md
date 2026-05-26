## 1. 项目结构创建

- [x] 1.1 创建 GPS 模块头文件 `Core/Inc/gps.h`
- [x] 1.2 创建 GPS 模块实现文件 `Core/Src/gps.c`

## 2. GPS 数据结构定义

- [x] 2.1 在 gps.h 中定义 GPS 数据结构体 GPS_SaveData
- [x] 2.2 定义缓冲区长度常量（GPS_Buffer_Length=80, UTCTime_Length=11 等）
- [x] 2.3 定义全局变量声明（rxdatabufer, point1, Save_Data）

## 3. USART2 串口配置

- [x] 3.1 实现 uart_init() 函数，初始化 USART2（波特率 9600）
- [x] 3.2 配置 PA2 为 USART2_RX，PA3 为 USART2_TX（复用功能）
- [x] 3.3 配置 USART2 参数：8 位数据位，1 位停止位，无校验，收发模式
- [x] 3.4 配置 NVIC，使能 USART2 中断
- [x] 3.5 实现 CLR_Buf() 函数，清空接收缓冲区

## 4. 串口中断服务程序

- [x] 4.1 实现 USART2_IRQHandler() 中断服务程序
- [x] 4.2 实现 `$` 起始符检测，清空缓冲区
- [x] 4.3 实现 GNRMC 语句识别（检测第 2-6 个字符为 GNRMC）
- [x] 4.4 实现 `\n` 结束符检测，触发数据保存
- [x] 4.5 实现缓冲区溢出保护（最大 200 字节）

## 5. NMEA 解析功能

- [x] 5.1 实现 parseGpsBuffer() 函数，解析 GNRMC 语句
- [x] 5.2 使用 strstr() 定位逗号分隔符
- [x] 5.3 提取第 1 个字段：UTC 时间
- [x] 5.4 提取第 2 个字段：定位状态（A/V）
- [x] 5.5 提取第 3 个字段：纬度
- [x] 5.6 提取第 4 个字段：纬度方向（N/S）
- [x] 5.7 提取第 5 个字段：经度
- [x] 5.8 提取第 6 个字段：经度方向（E/W）
- [x] 5.9 设置 isGetData、isParseData、isUsefull 标志位

## 6. 坐标转换功能

- [x] 6.1 实现坐标转换函数（度分格式 → 十进制度）
- [x] 6.2 实现纬度转换：ddmm.mmmm → dd + mm.mmmm/60
- [x] 6.3 实现经度转换：dddmm.mmmm → ddd + mm.mmmm/60
- [x] 6.4 确保转换结果保留 6 位小数精度

## 7. 错误处理

- [x] 7.1 实现 errorLog() 函数，处理解析错误
- [x] 7.2 实现 clrStruct() 函数，清空结构体和标志位
- [x] 7.3 添加字段缺失检测和处理
- [x] 7.4 添加数据格式错误检测和处理

## 8. 辅助函数

- [x] 8.1 实现 Hand() 函数，串口命令识别（可选）
- [x] 8.2 实现 printGpsBuffer() 函数，通过串口打印调试信息

## 9. 主程序集成

- [x] 9.1 在 main.h 中包含 gps.h 头文件
- [x] 9.2 在 main() 函数中调用 uart_init(9600) 初始化 GPS 串口
- [x] 9.3 在 main() 函数中调用 clrStruct() 初始化 GPS 数据结构
- [x] 9.4 在主循环 while(1) 中添加 parseGpsBuffer() 调用
- [x] 9.5 在主循环中添加 printGpsBuffer() 调用（调试用）

## 10. LCD 显示集成

- [x] 10.1 在 lcd_display.h 中添加 GPS 显示函数声明
- [x] 10.2 在 lcd_display.c 中实现 LCD_DisplayGPS() 函数
- [x] 10.3 实现定位状态显示（GPS: OK 或 GPS: NO FIX）
- [x] 10.4 实现纬度信息显示（LAT: xx.xxxxxx N/S）
- [x] 10.5 实现经度信息显示（LON: xxx.xxxxxx E/W）
- [x] 10.6 实现 UTC 时间显示（UTC: hh:mm:ss）
- [x] 10.7 在主循环中调用 LCD_DisplayGPS() 更新显示
- [x] 10.8 实现分区显示，GPS 和声音检测互不干扰
- [x] 10.9 优化显示刷新策略，避免屏幕闪烁

## 11. 测试验证

- [ ] 11.1 编译项目，确保无编译错误和警告
- [ ] 11.2 连接 ATGM336H GPS 模块（PA2-RX, PA3-TX, VCC-3.3V, GND）
- [ ] 11.3 户外测试，验证能正常接收 GNRMC 数据
- [ ] 11.4 验证坐标转换准确性（与手机地图对比）
- [ ] 11.5 验证 LCD 显示效果（清晰度、布局）
- [ ] 11.6 测试定位失效时的显示（NO FIX 提示）

## 12. 文档和清理

- [x] 12.1 添加代码注释（函数功能、参数说明）
- [x] 12.2 清理调试代码（如不需要，移除 printGpsBuffer 调用）
- [x] 12.3 更新 README 或添加 GPS 模块使用说明
- [x] 12.4 记录已知问题和后续改进建议
