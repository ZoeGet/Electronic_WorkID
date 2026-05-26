## 1. CubeMX 配置

- [ ] 1.1 使能 `ADC1_IN1(PC0)`，外部触发源设为 `TIM2 TRGO`
- [ ] 1.2 配置 `TIM2` 产生 16kHz 更新事件（`PSC=0, ARR=4999`，以 80MHz 为例）
- [ ] 1.3 配置 ADC DMA 循环传输，开启 Half/Full 回调
- [ ] 1.4 使能 `SDMMC1` 1-bit（`PD2/PC12/PC8`），开启 DMA Tx/Rx
- [ ] 1.5 使能 FATFS（SD 驱动），确认 `f_mount` 可成功

## 2. 录音模块骨架

- [ ] 2.1 新建 `recorder.h/.c`，定义状态机与外部接口
- [ ] 2.2 新建 `wav_format.h/.c`，实现 WAV 头生成与回填
- [ ] 2.3 新建 `audio_buffer.h/.c`，实现双缓冲与块状态管理
- [ ] 2.4 在 `main.c` 接入 `Recorder_Init/Start/Process/Stop`

## 3. DMA 与缓存协同

- [ ] 3.1 在 `HAL_ADC_ConvHalfCpltCallback` 标记前半块 ready
- [ ] 3.2 在 `HAL_ADC_ConvCpltCallback` 标记后半块 ready
- [ ] 3.3 主循环消费 ready 块并调用 `f_write`
- [ ] 3.4 加入 overrun 检测与统计

## 4. FATFS 写入优化

- [ ] 4.1 录音开始写入 44B 占位 WAV 头
- [ ] 4.2 启用 512B 对齐批量写入
- [ ] 4.3 使用 `f_lseek` 预分配空间并回到数据起点
- [ ] 4.4 周期性 `f_sync` 提升掉电安全
- [ ] 4.5 停止时回填头并关闭文件

## 5. 联调与测试

- [ ] 5.1 验证 SD 卡创建文件、写入和关闭流程
- [ ] 5.2 验证 10s/60s 连续录音文件可播放
- [ ] 5.3 验证录音过程中无 overrun 与写入错误
- [ ] 5.4 验证异常场景（拔卡、空间不足）错误码与恢复流程
