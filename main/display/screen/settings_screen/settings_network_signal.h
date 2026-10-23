#pragma once

#include <lvgl.h>

/** @brief 创建实时网络信号栏，控件删除时停止采样；同一时间仅支持一个可见信号栏。 */
void SettingsNetworkSignal_Build(lv_obj_t* parent);
