# Metalio E-Ink4 Plus：低功耗与蓝牙音频

## 浅睡兼容处理

ESP32-S31 的浅睡唤醒需要完成 MPLL 校准和 PSRAM 时钟恢复。兼容模块在每次唤醒校准前恢复必要的 DREG/PVT 配置，并补齐 PSRAM/MPLL 时钟交接；升级 ESP-IDF 时应核对这些处理是否仍然适用。

## 模块实现

### 代码组织

SDK 兼容修复与 RTC 诊断集中在公共电源目录的 `s31_sleep/` 模块中：

- `s31_sleep.cc`：MPLL 包装、启动配置、RTC 快照和延迟日志重放。
- `s31_sleep.h`：初始化、复位报告、阶段记录和浅睡调用接口；阶段使用具名枚举。
- `s31_sleep_sdk.cmake`：统一接入模块源码、链接包装及构建期 SDK 兼容补丁。

`power_hw.cc` 保留板级电源流程，通过这些接口调用兼容模块，
不直接访问 MPLL 寄存器、SDK 包装状态或 RTC 快照结构。
模块保留原日志标签、阶段值、RTC 布局与寄存器操作顺序。
清空构建目录后会重新生成兼容源码，本机安装的 IDF 不受修改。

### 每次唤醒校准前恢复必要配置

在独立模块 [s31_sleep.cc](../common/power_policy/s31_sleep/s31_sleep.cc) 包装 MPLL 校准入口：

- 仅在 SDK 的 PMU 浅睡恢复路径应用兼容处理。
- 每次校准前恢复 DREG/PVT 为 `12/12`，等待配置稳定。
- 补齐 MPLL 模拟初始化，再执行实际校准，并保存前后寄存器快照。
- 在缓存可能关闭、另一 CPU 暂停的阶段使用 IRAM 和底层寄存器访问，避免日志、分配及跨核锁。
- 睡眠返回后清除作用域标志，避免影响正常运行中的时钟配置。

模拟初始化和启动配置参考 Espressif 官方修复
[fix(mpll): improved mpll calibration flow on esp32s31](https://github.com/espressif/esp-idf/commit/9726078b55430ef8d2ce4a460ebf21d889bb5c12)。

### 补齐 PSRAM 与 MPLL 时钟交接

[s31_sleep_sdk.cmake](../common/power_policy/s31_sleep/s31_sleep_sdk.cmake) 在构建目录生成 SDK 兼容源码，
不修改本机安装的全局 SDK：

- 入睡前将 PSRAM 时钟切换到 XTAL，并按条件处理 MSPI 时钟，再关闭 MPLL。
- 唤醒后先恢复 MPLL 和 MSPI 时钟，再将 PSRAM 时钟切回 MPLL，之后恢复 PSRAM 访问。
- SDK 已包含对应交接逻辑时跳过该兼容补丁；源码匹配失败时停止构建，要求人工复核。

参考官方当前 [ESP32-S31 PMU 实现](https://github.com/espressif/esp-idf/blob/master/components/esp_hw_support/port/esp32s31/pmu_sleep.c)。
升级 SDK 后应重新核对兼容补丁与包装逻辑，避免重复应用。

### 保留保护与补充日志

RTC 看门狗保持启用，CPU retention 和 PSRAM halfsleep 保持原配置。

RTC 保留区保存睡眠阶段和寄存器快照；启动时读取上次记录，并在启动后
10、30、60 秒重放，方便 USB 重新枚举后获取复位证据。
阶段 `9` 表示已完成硬件恢复；阶段 `501` 表示即将进入实际 MPLL 校准，
`502` 表示校准已返回。应结合复位原因和后续记录判断，不能只看一条阶段日志。

更多电源策略与诊断入口说明见 [公共电源策略 README](../common/power_policy/README.md)。

## 蓝牙音频初始化

开机由独立后台任务先复位 BH，再下发模式1配置；复位、电源稳定等待、应答等待和重试均不阻塞主事件循环。
只有收到明确的 `SET MODE 1` 应答才判定就绪，串口写入成功或收到任意数据不代表初始化成功。
首次失败后最多重试3次，每次间隔1秒；同一电源周期重试耗尽后保持失败，不因页面轮询无限重试。
重新上电会使上次确认结果失效。AI、录音测试和老化测试均在后台等待模式确认，再按需申请 I2S 资源。
离开音频功能后释放 I2S 通道与 DMA；蓝牙页面替换数据回调不会移除初始化流程的应答观察者。

初始化失败仅保留失败阶段和尝试次数等必要错误信息。
