#pragma once

/** 热点与文件页地址，供传输页画二维码 */
struct ApTransferInfo {
    char ssid[33]; // Metalio- 加 MAC 后 4 位十六进制
    char url[48];  // 浏览器文件页
    bool ok;       // 热点和网页已起来
    const char* fail; // 失败原因；成功为空
};

/**
 * @brief 开无密码热点并提供整卡文件页
 * @note 支持选择目录或自动分类上传；离开传输页时 ApTransferStop 恢复 STA
 */
bool ApTransferStart(ApTransferInfo* info);
/**
 * @brief 关闭热点与文件页
 * @param resume_sta true 时恢复进入前的 STA；切到 U 盘时传 false，射频保持关闭
 */
void ApTransferStop(bool resume_sta = true);
/** @brief 非阻塞请求停止接收和写入；standby 为真时同时禁止热点启动，实际收尾由后台调用 Stop */
void ApTransferRequestStop(bool standby = false);
/** @brief 退出待机后允许再次打开热点 */
void ApTransferAllowStart();

/** @brief 上传存储发生 I/O 故障且已结束收尾，需要提示重启 */
bool ApTransferNeedsStorageRestart();
