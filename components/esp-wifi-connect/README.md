# ESP32 Wi-Fi Connect

本项目维护的 STA 连接组件，提供已保存网络的连接、重连、SSID 管理和扫网租约。
Wi-Fi 凭证由「设置 → 网络 → 配置 WIFI 网络」机上界面维护，存储在 NVS 的 `wifi` 命名空间。
未保存网络或连接失败时，不启动网页配网热点；原网页、配网 AP、SmartConfig 及专用 DNS 已移除。
传输页的 AP 文件管理由 `main/display/screen/cloud_screen/ap/` 独立实现，见 [AP 文件管理](../../docs/ap-file-transfer.md)。

已有凭证时通过 `WifiStation::GetInstance().Start()` 启动 STA；浅睡暂停和唤醒恢复使用其 `PauseForLp()` / `ResumeFromLp()`。

## 兼容性与 OTA 恢复

当前 STA 配置关闭 Wi-Fi 省电和 802.11ax，保留 b/g/n 自动速率。470 的 STA 和 AP 文件传输使用固定 TX 功率请求上限 12 dBm（SDK 原始值 48，无界面配置）。S31 的 Wi-Fi/lwIP 缓冲、邮箱及 TX/RX 聚合按对照启动日志调整，具体参数及 SDK 约束见 [AP 文件管理](../../docs/ap-file-transfer.md)。

连接采用全信道扫描和 RSSI 排序，不固定 BSSID。驱动可能回退到其他候选 AP，不能保证总是选择此前扫描记录中信号最强的 AP。

TCP 分段由协议栈处理。独立日志上传不再修改 TCP MSS，也不再强制单次写入上限为 512 字节；仍按 1 KiB 读取发送，处理部分写入并保留超时和取消检查。

OTA 版本检测和激活配置检查使用原自定义 HTTP 客户端。每次检查最多尝试 3 次，单次预算 8 秒、总预算 30 秒，重试等待 500/1000 毫秒，每次重建 HTTP 连接。只有网络未就绪才调用现有网络恢复逻辑，不强制重新关联仍连接的 Wi-Fi。网络错误、超时、无效响应及 408/429/5xx 可重试，其他 HTTP 错误直接返回。预算不是硬实时保证，底层调用和任务调度可能产生额外耗时。

