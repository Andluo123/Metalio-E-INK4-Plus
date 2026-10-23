#pragma once

#include "lvgl.h"

namespace reader { struct RasterImage; }

// 待机壁纸：解码 NVS「standby」指向的图片，Park 时复用关机图的可绘区帧缓冲绘制。
// 仅由 StandbyScreen 入口在缓存判定有可用待机壁纸时调用。

/** @brief 在 root 上构建白底；不创建图片控件，不读 NVS。 */
void StandbyWallpaper_Build(lv_obj_t* root);

/**
 * @brief 后台解析待机壁纸路径并解码，经 lv_async 接收像素供 Park 绘制。
 * 须在 Build 之后调用；Dismiss/Teardown 后结果按 epoch 丢弃。
 */
void StandbyWallpaper_StartLoad();

/** @brief 解码完成（成功或失败）；Park 前查询，勿固定等 settle。 */
bool StandbyWallpaper_IsContentReady();

/** @brief LVGL 任务在 Park 时读取；像素由本模块持有，直到 Teardown。 */
const reader::RasterImage* StandbyWallpaper_DecodedImage();

/** @brief 拆掉图片引用并释放像素；可重复调用。 */
void StandbyWallpaper_Teardown();

/** @brief 丢弃已解码像素；下次 StartLoad 必重读文件（壁纸覆盖后调用）。 */
void StandbyWallpaper_InvalidateDecoded();
