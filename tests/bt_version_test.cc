// 蓝牙版本查询：AT 应答解析 + 查询状态机判定
//
// 纯 host 测试，不依赖 ESP-IDF / 硬件：
//   cl /nologo /utf-8 /EHsc /std:c++17 tests\bt_version_test.cc
// 或 tools/tests/run_host_tests.ps1

#include "../main/boards/metalio-e-ink4-plus/periph/bt_at_parser.h"
#include "../main/boards/metalio-e-ink4-plus/periph/bt_version_policy.h"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace {

using bt_version::AcceptResult;
using bt_version::DecideQuery;
using bt_version::QueryContext;
using bt_version::QueryDecision;

void ExpectVersion(const char* line, const char* expected) {
    char out[64]{};
    const bool ok = bt_at::ParseVersionLine(line, out, sizeof(out));
    assert(ok && "expected parse success");
    assert(std::strcmp(out, expected) == 0 && "unexpected version text");
}

void ExpectReject(const char* line) {
    char out[64]{};
    std::memset(out, 0x5A, sizeof(out));
    assert(!bt_at::ParseVersionLine(line, out, sizeof(out)) && "expected parse failure");
    // 失败时不得写出半截版本号，否则 UI 会把脏数据当结果显示。
    assert(out[0] == char(0x5A) && "output must stay untouched on failure");
}

void TestParseVersionLine() {
    // 正常应答
    ExpectVersion("AT+GVER=1.2.3", "1.2.3");
    ExpectVersion("AT+GVER=V1.0.7", "V1.0.7");
    ExpectVersion("AT+GVER=  BH1098G_2.1  ", "BH1098G_2.1");
    ExpectVersion("AT+GVER=2.1.0+build5", "2.1.0+build5");

    // 命令回显 / 空值 / 非版本串
    ExpectReject("AT+GVER=1");
    ExpectReject("AT+GVER=");
    ExpectReject("AT+GVER=   ");
    ExpectReject("AT+GVER=abc");
    ExpectReject("AT+GVER=1.0 beta");
    ExpectReject("AT+GVERX=1.2.3");
    ExpectReject("OK");
    ExpectReject("");
    ExpectReject(nullptr);

    // 缓冲不足：宁可失败也不能显示被截断的版本
    {
        char small[4]{};
        assert(!bt_at::ParseVersionLine("AT+GVER=1.2.3", small, sizeof(small)));
    }
    // 参数非法
    {
        char out[8]{};
        assert(!bt_at::ParseVersionLine("AT+GVER=1.2.3", nullptr, sizeof(out)));
        assert(!bt_at::ParseVersionLine("AT+GVER=1.2.3", out, 0));
    }
}

void TestQueryDecision() {
    const auto decide = [](bool uart, bool powered, bool p14) {
        return DecideQuery(QueryContext{uart, powered, p14});
    };

    assert(decide(true, true, true) == QueryDecision::kSend);
    assert(decide(false, true, true) == QueryDecision::kReject);
    assert(decide(true, false, true) == QueryDecision::kReject);
    assert(decide(true, true, false) == QueryDecision::kRetryPower);
    // UART 未初始化时即使 P14 未就绪也应直接失败，避免无意义的重试循环
    assert(decide(false, true, false) == QueryDecision::kReject);
    assert(decide(true, false, false) == QueryDecision::kReject);
}

void TestResultAcceptance() {
    // 查询期间电源周期没变 → 接受
    assert(AcceptResult(7, 7, true));
    // 查询途中 P14 被切走（周期 +1）→ 丢弃
    assert(!AcceptResult(7, 8, true));
    // 当前已断电 → 丢弃
    assert(!AcceptResult(7, 7, false));

    // 事件序列：查询 → 断电 → 恢复 → 重新查询
    unsigned epoch = 7;
    const unsigned query_epoch = epoch;
    epoch += 1;  // P14 掉电
    assert(!AcceptResult(query_epoch, epoch, true));
    epoch += 0;  // 恢复供电不改变周期号
    assert(AcceptResult(epoch, epoch, true));
}

}  // namespace

int main() {
    TestParseVersionLine();
    TestQueryDecision();
    TestResultAcceptance();
    std::puts("bt_version: AT 应答解析 + 查询状态机 检查通过");
    return 0;
}
