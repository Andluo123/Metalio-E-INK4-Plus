#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** L8 image envelope (header + w*h bytes), shared by local images and cached downloads. */
#define A2UI_L8_MAGIC 0x384C3241u /* 'A2L8' little-endian */

/**
 * Soft cap on concurrent displayed surfaces (RAM / PSRAM).
 * Tuned for e-ink assistant pages (typically 1 image/page); 3 leaves headroom
 * for rare multi-image updates without holding 8 full bitmaps.
 * Exceeding it skips further load_async for that update (placeholders remain).
 */
#define A2UI_IMG_MAX_SURFACES 3

void a2ui_image_set_disp(lv_display_t *disp);

/** Base like http://192.168.x.x:8765 — used to rewrite localhost / relative Image.url */
void a2ui_image_set_http_origin(const char *origin);

/**
 * Load image for url onto img_obj.
 *
 * Architecture
 * ------------
 * - One surface per lv_image (owned pixel buffer + dsc); up to A2UI_IMG_MAX_SURFACES.
 * - HTTP / file://：异步读取 A2I1/A2L8/PNG/JPEG，内容亮度保留为 L8
 * - mem://：在已持的 LVGL 锁内同步上屏（A2L8 直显，或 A2I1→L8）
 * - 内容通过角色声明按需使用灰阶；普通 UI 仍为黑白
 *
 * Tear-down order under LVGL lock:
 *   1) a2ui_image_abort_loads()  — cancel worker unit, keep pixels
 *   2) destroy lv_image objects     — drop references to surface dsc
 *   3) a2ui_image_clear()         — free all surface buffers
 */
esp_err_t a2ui_image_load_async(lv_obj_t *img_obj, const char *url, int max_w, int max_h);

/**
 * Register an in-memory A2I1 blob under `url` (e.g. mem://assistant/shot_1).
 * Copied to SPIRAM; same url overwrites. Survives abort/clear so page-flip can reload.
 */
esp_err_t a2ui_image_put_local(const char *url, const uint8_t *a2i1, size_t len);

/**
 * Register L8 pixels (0=黑 255=白) as mem:// blob（A2L8 封装）。
 * 百问拍照等本地图应走此接口，避免 A2I1 编解码后再上屏失败成线框。
 */
esp_err_t a2ui_image_put_local_l8(const char *url, uint16_t w, uint16_t h, const uint8_t *pixels,
                                  size_t pixels_len);

/** Cancel in-flight/queued loads (gen++); does not free displayed pixel buffers. */
void a2ui_image_abort_loads(void);

/** Abort loads and free all surface buffers. Call only after lv_image objs are gone. */
void a2ui_image_clear(void);

#ifdef __cplusplus
}
#endif
