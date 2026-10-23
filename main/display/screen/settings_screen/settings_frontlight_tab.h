#pragma once

#include "lvgl.h"

/** @brief 构建设置→屏幕页的刷新模式及前光选项 */
void SettingsFrontlightTab_Build(lv_obj_t* page);
/** @brief 重置前光页 UI 指针 */
void SettingsFrontlightTab_Reset();
/** @brief 唤醒后同步前光亮度控件 */
void SettingsFrontlightTab_Refresh();
