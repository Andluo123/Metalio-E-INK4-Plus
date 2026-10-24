#pragma once

// 蓝牙版本查询的状态判定。
//
// 这些判断以前散落在 QueryBtVersionOnce / GetBtVersion 里，只能靠真机
// 复现“查询中电源被切走”这类竞态。抽成纯函数后可在 PC 上做回归测试。

namespace bt_version {

struct QueryContext {
    bool uart_initialized;  // UART 已 begin 并注册 observer
    bool powered;           // 本电源周期 BH 供电标记
    bool p14_ready;         // P14 轨道状态机已就绪
};

enum class QueryDecision {
    kSend,        // 可以发 AT+GVER=1
    kRetryPower,  // P14 还没稳定（或正在复位），稍后重试
    kReject,      // UART 未初始化或本周期已断电，直接失败
};

inline QueryDecision DecideQuery(const QueryContext& ctx) {
    if (!ctx.uart_initialized || !ctx.powered) {
        return QueryDecision::kReject;
    }
    if (!ctx.p14_ready) {
        return QueryDecision::kRetryPower;
    }
    return QueryDecision::kSend;
}

/**
 * @brief 查询结果能否写入缓存
 *
 * 必须同时满足：查询期间的电源周期没变、且当前仍处于供电状态。
 * 否则说明查询过程中 P14 被切走，拿到的可能是上一次上电的残值。
 */
inline bool AcceptResult(unsigned query_epoch, unsigned current_epoch, bool powered) {
    return powered && query_epoch == current_epoch;
}

}  // namespace bt_version
