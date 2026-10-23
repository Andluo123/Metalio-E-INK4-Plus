#pragma once

#include <lvgl.h>

constexpr int kApQrSide = 240; // 二维码边长（含静区）
constexpr int kApQrPad = 3;    // 黑框与二维码的间隙
constexpr int kApQrRadius = 16; // 外框圆角
constexpr int kApQrBorder = 2;  // 外框线宽

/** @brief 把文本画成二维码；画布需已设为 I1 缓冲 */
void ApQrPaint(lv_obj_t* canvas, const char* text);
