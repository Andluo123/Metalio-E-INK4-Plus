#pragma once

#include <vector>

#include <lvgl.h>

#include "cloud_screen/push/push_resources_library.h"

/** @brief 关闭通用对话框 */
void CloseDialog();
/** @brief 关闭动作确认框 */
void CloseActionDialog();
/** @brief 显示动作确认框 */
void ShowActionDialog(int cloud_index);
/** @brief 显示同名文件处理弹窗 */
void ShowSaveConflictDialog(int cloud_index);
/** @brief 调度删除选中云资源 */
void ScheduleDeleteItems(std::vector<reader::CloudPushResource> items);
/** @brief 云页删除按钮 */
void Cloud_OnDeleteClicked(lv_event_t* e);

/** @brief 显示内存卡故障及重启按钮，重复调用不叠加弹窗 */
void ShowStorageErrorDialog();
