#pragma once

#include "lvgl.h"

/** @brief 构建传输页 U 盘区（启用按钮、容量与状态提示） */
void SettingsStorageTab_Build(lv_obj_t* page);
/** @brief 重置存储页并关闭虚拟 U 盘 UI 通知 */
void SettingsStorageTab_Reset();
