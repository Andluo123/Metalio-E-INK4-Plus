#pragma once

#include <stddef.h>
#include <stdint.h>

class AudioCodec;

/**
 * @brief 返回板级 AudioCodec（转发 Board::GetAudioCodec）
 */
AudioCodec* MetalioAudio_GetCodec();

/**
 * @brief 初始化 UART，后台先复位 BH，再配置并确认模式1；不会等待复位完成
 */
void MetalioAudio_Init();
/**
 * @brief 请求后台初始化；同一电源周期重试耗尽后不重复启动
 */
void MetalioAudio_RequestModeInit();
constexpr int kMetalioAudioInitWaitMs = 20000; // 后台调用方等待初始化的最长时间

/**
 * @brief 电源关闭时使已确认的音频模式失效
 */
void MetalioAudio_OnPowerOff();
/**
 * @brief 记录恢复供电时间，供后台初始化等待电源稳定
 */
void MetalioAudio_OnPowerRestored();

/**
 * @brief 等待 AT 模式1 就绪
 * @param timeout_ms 最长等待毫秒；等待操作仅供后台任务使用
 * @return true 已就绪
 */
bool MetalioAudio_WaitModeReady(int timeout_ms = 2000);

/**
 * @brief 模式已就绪时启动 codec，否则请求后台初始化并立即返回
 * @return true 已启动；false 尚未就绪或启动失败
 */
bool MetalioAudio_EnsureStarted();

/**
 * @brief 当前电源周期已收到明确的 SET MODE 1 成功应答
 */
bool MetalioAudio_ModeReady();

/**
 * @brief 设置板级功放使能状态
 */
void MetalioAudio_SetPa(bool on);

/**
 * @brief 输入采样率 Hz
 */
int MetalioAudio_InputRate();

/**
 * @brief 读取已缓存的蓝牙音频芯片固件版本；不会触发 UART 查询
 * @param out 输出缓冲（UTF-8 文本）
 * @param out_len 缓冲大小
 * @return true 缓存有效；false 未知或缓存已失效
 */
bool MetalioAudio_GetCachedBtVersion(char* out, size_t out_len);

/**
 * @brief 查询蓝牙音频芯片（BH1098G）固件版本；结果缓存，重复调用直接返回
 * @param out 输出缓冲（UTF-8 文本）
 * @param out_len 缓冲大小
 * @param timeout_ms 单次 AT 应答等待上限；≤0 用默认 1500ms
 * @return true 已取得版本字符串；false 未知/超时/未初始化
 */
bool MetalioAudio_GetBtVersion(char* out, size_t out_len, int timeout_ms = 0);

/**
 * @brief 清除蓝牙版本缓存（下次 GetBtVersion 重新查询）
 */
void MetalioAudio_ClearBtVersionCache();

/**
 * @brief 读取音频输入 PCM，声道布局由板级 codec 决定
 * @return 实际采样点数；超时/失败 ≤0
 */
int MetalioAudio_ReadMic(int16_t* dest, int samples);

/**
 * @brief 写喇叭 PCM（单声道 int16）；内部开 PA
 * @return 写入采样点数；失败 ≤0
 */
int MetalioAudio_WriteSpk(const int16_t* data, int samples);
