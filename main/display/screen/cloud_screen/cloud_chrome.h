#pragma once

#include <lvgl.h>

/** @brief 调度用户触发的云同步 */
void ScheduleUserCloudSync();
/** @brief 云页 Tab 点击 */
void OnTabClicked(lv_event_t* e);
/** @brief 创建 Tab 按钮 */
lv_obj_t* MakeTabBtn(lv_obj_t* parent, const char* text, int tab);
/** @brief 同步按钮点击 */
void OnSyncClicked(lv_event_t* e);
/** @brief 构建云页主 chrome */
void BuildCloudMainChrome(lv_obj_t* scr, lv_coord_t status_height);
/** @brief 切换传输方式并启停热点或 U 盘页 */
void EnterCloudMode(int mode);
/** @brief 离开传输页：使旧结果失效，非阻塞请求后台关闭传输并恢复 STA */
void CloudRadioAbandon();
/** @brief 电源键进待机：停掉热点，页还留着 */
void ParkApForStandby();
/** @brief 退出待机：AP 页重新打开热点 */
void ReloadApAfterStandby();
/** @brief 构建壁纸预览 chrome */
void BuildWallpaperPreviewChrome(lv_obj_t* scr, lv_coord_t status_height);
