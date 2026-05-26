## ADDED Requirements

### Requirement: TTS 提示音基于 DAC DMA 播放
系统 SHALL 使用 `DAC1 Channel 1`、`TIM6 TRGO` 和 DMA 播放预生成的 TTS 提示音数据。

#### Scenario: 播放开始录音提示音
- **WHEN** 应用请求播放“开始录音”提示音
- **THEN** 系统应通过 `DAC1_OUT1(PA4)` 输出对应的 TTS 语音波形

#### Scenario: 播放停止录音提示音
- **WHEN** 应用请求播放“录音已停止”提示音
- **THEN** 系统应通过 `DAC1_OUT1(PA4)` 输出对应的 TTS 语音波形

#### Scenario: 使用 16kHz 采样节拍
- **WHEN** TTS 播放开始
- **THEN** `TIM6` 应产生 16kHz 更新事件作为 DAC 触发源

### Requirement: 提示音播放应为一次性非循环播放
系统 SHALL 使用一次性 DMA 传输播放提示音，播放完成后自动停止。

#### Scenario: 播放完成自动停止
- **WHEN** DMA 已传输完整个提示音数组
- **THEN** 系统应停止 `TIM6` 和 `DAC DMA`
- **AND** 系统应将播放忙碌状态清除

#### Scenario: 播放后恢复静音中点
- **WHEN** 提示音播放完成或被强制停止
- **THEN** DAC 输出值应恢复到 `2048` 附近的静音中点

### Requirement: 播放接口不得阻塞主循环
系统 SHALL 提供非阻塞播放接口，播放期间主循环仍可继续处理录音、GPS、LCD 和错误状态。

#### Scenario: 播放启动立即返回
- **WHEN** 应用调用任一 TTS 播放接口
- **THEN** 播放接口应在启动 DAC DMA 后返回，不应等待整段语音播放完成

#### Scenario: 播放状态可查询
- **WHEN** 应用需要判断提示音是否播放完成
- **THEN** 系统应提供 busy 状态查询接口

### Requirement: 播放中重复请求应被安全处理
系统 SHALL 安全处理播放期间的新播放请求。

#### Scenario: 播放中再次请求播放
- **WHEN** TTS 正在播放且应用再次请求播放提示音
- **THEN** 系统应拒绝新的播放请求并保持当前播放不被打断

#### Scenario: 播放失败可恢复
- **WHEN** `HAL_DAC_Start_DMA()` 或 `HAL_TIM_Base_Start()` 返回失败
- **THEN** 系统应清除播放忙碌状态并返回失败结果

### Requirement: 开始录音提示音不应进入录音文件
系统 SHALL 在 KEY0 开始录音流程中先播放“开始录音”提示音，待提示音播放完成后再启动录音。

#### Scenario: KEY0 触发开始录音
- **WHEN** 用户按下 KEY0 且系统允许开始录音
- **THEN** 系统应先播放“开始录音”提示音
- **AND** 在提示音播放完成前不应启动 ADC 录音链路

#### Scenario: 提示音完成后启动录音
- **WHEN** “开始录音”提示音播放完成
- **THEN** 系统应执行录音初始化与开始录音流程
- **AND** LCD 应显示录音开始状态

### Requirement: 停止录音后播放停止提示音
系统 SHALL 在 KEY1 停止录音成功后播放“录音已停止”提示音。

#### Scenario: KEY1 触发停止录音
- **WHEN** 用户按下 KEY1 且系统处于录音状态
- **THEN** 系统应先停止录音并完成 WAV 头回填
- **AND** 停止成功后播放“录音已停止”提示音

#### Scenario: 停止失败不播放成功提示音
- **WHEN** `Recorder_Stop()` 返回失败
- **THEN** 系统不应播放“录音已停止”成功提示音
- **AND** 系统可播放错误提示音或显示错误状态

### Requirement: 硬件输出必须通过功放播放
系统 SHALL 将 `PA4 / DAC1_OUT1` 作为模拟音频信号源，后级通过功放驱动喇叭。

#### Scenario: 外接功放播放
- **WHEN** TTS 提示音播放
- **THEN** 用户应能通过连接到 DAC 输出后级的功放和喇叭听到语音提示

#### Scenario: 不直接驱动无源喇叭
- **WHEN** 设计硬件连接
- **THEN** 不应将无源喇叭直接连接到 `PA4 / DAC1_OUT1`
