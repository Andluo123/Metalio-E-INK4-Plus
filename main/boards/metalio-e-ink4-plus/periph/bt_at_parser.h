#pragma once

// BH1098G 的 AT 应答解析。
//
// 单独抽成无 ESP-IDF 依赖的头文件，是为了能在 PC 上直接跑单元测试：
// 这里的行为（前缀匹配、空白裁剪、命令回显过滤）曾经出过错，
// 而且只有真实芯片才会给出“奇怪但合法”的版本串，必须可回归。

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace bt_at {

constexpr const char* kVersionPrefix = "AT+GVER=";
constexpr size_t kVersionPrefixLen = sizeof("AT+GVER=") - 1;

/**
 * @brief 从一行 AT 应答中提取固件版本
 *
 * 只接受以 "AT+GVER=" 开头的行；版本值裁剪首尾空白后必须至少含一个数字，
 * 且只允许 [A-Za-z0-9._+-]。值为单个 "1" 视为命令回显，丢弃。
 *
 * @param line     已去掉 CR/LF 的应答行（不能为 nullptr）
 * @param out      输出缓冲
 * @param out_len  缓冲大小
 * @return true 解析成功
 */
inline bool ParseVersionLine(const char* line, char* out, size_t out_len) {
    if (line == nullptr || out == nullptr || out_len == 0) {
        return false;
    }
    if (std::strncmp(line, kVersionPrefix, kVersionPrefixLen) != 0) {
        return false;
    }
    const char* value = line + kVersionPrefixLen;
    while (*value == ' ' || *value == '\t') {
        ++value;
    }
    size_t value_len = std::strlen(value);
    while (value_len > 0 && (value[value_len - 1] == ' ' || value[value_len - 1] == '\t')) {
        --value_len;
    }
    // 排除命令回显；正常版本号不应恰好等于查询参数 "1"。
    if (value_len == 0 || (value_len == 1 && value[0] == '1')) {
        return false;
    }
    bool has_digit = false;
    for (size_t i = 0; i < value_len; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c >= '0' && c <= '9') {
            has_digit = true;
        } else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     c == '.' || c == '-' || c == '_' || c == '+')) {
            return false;
        }
    }
    if (!has_digit) {
        return false;
    }
    // 静默截断的版本号比查询失败更危险（会显示成错误型号），直接判失败。
    if (value_len + 1 > out_len) {
        return false;
    }
    std::snprintf(out, out_len, "%.*s", static_cast<int>(value_len), value);
    return true;
}

}  // namespace bt_at
