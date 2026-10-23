#include "home_icon_source.h"
#include <lvgl.h>
#include "src/draw/lv_image_decoder_private.h"
#include "esp_heap_caps.h"
#include <algorithm>

namespace {
struct Session {
    const lv_image_dsc_t* source;
    int32_t next_y;
    lv_draw_buf_t tile;
};
lv_result_t Info(lv_image_decoder_t*, lv_image_decoder_dsc_t* dsc, lv_image_header_t* header) {
    if(dsc->src_type != LV_IMAGE_SRC_VARIABLE) return LV_RESULT_INVALID;
    const auto* source = static_cast<const lv_image_dsc_t*>(dsc->src);
    if(source->header.cf != LV_COLOR_FORMAT_RAW || source->header.flags != LV_IMAGE_FLAGS_USER8 ||
       !source->header.w || !source->header.h || !source->data ||
       source->data_size < uint32_t(source->header.stride) * source->header.h) return LV_RESULT_INVALID;
    *header = source->header;
    header->cf = LV_COLOR_FORMAT_ARGB8888;
    header->flags = 0;
    return LV_RESULT_OK;
}
lv_result_t Open(lv_image_decoder_t*, lv_image_decoder_dsc_t* dsc) {
    auto* session = static_cast<Session*>(heap_caps_calloc(1, sizeof(Session), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if(!session) return LV_RESULT_INVALID;
    session->source = static_cast<const lv_image_dsc_t*>(dsc->src);
    dsc->user_data = session;
    dsc->args.no_cache = true;
    dsc->decoded = nullptr;
    return LV_RESULT_OK;
}
lv_result_t Area(lv_image_decoder_t*, lv_image_decoder_dsc_t* dsc, const lv_area_t* full, lv_area_t* decoded) {
    auto* session = static_cast<Session*>(dsc->user_data);
    if(!session || full->x1 > full->x2 || full->y1 > full->y2) return LV_RESULT_INVALID;
    const auto* source = session->source;
    const int32_t x1 = std::max<int32_t>(full->x1, 0);
    const int32_t x2 = std::min<int32_t>(full->x2, source->header.w - 1);
    const int32_t y = std::max<int32_t>(session->next_y,
        decoded->y1 == LV_COORD_MIN ? full->y1 : decoded->y2 + 1);
    if(x1 > x2 || y < 0 || y > full->y2 || y >= source->header.h) return LV_RESULT_INVALID;
    // Keep SPNG's 16-row boundaries so scaling produces the same pixels.
    const int32_t end = std::min({((y / 16) + 1) * 16, int32_t(source->header.h), full->y2 + 1});
    auto& tile = session->tile;
    tile.header = dsc->header;
    tile.header.w = x2 - x1 + 1;
    tile.header.h = end - y;
    tile.data = const_cast<uint8_t*>(source->data + y * source->header.stride + x1 * 4);
    tile.data_size = (tile.header.h - 1) * source->header.stride + tile.header.w * 4;
    tile.unaligned_data = tile.data;
    tile.handlers = lv_draw_buf_get_image_handlers();
    *decoded = {full->x1, y, full->x2, end - 1};
    session->next_y = end;
    dsc->decoded = &tile;
    return LV_RESULT_OK;
}
void Close(lv_image_decoder_t*, lv_image_decoder_dsc_t* dsc) {
    heap_caps_free(dsc->user_data);
    dsc->user_data = nullptr;
}
}
bool HomeIconDecoderReady() {
    static lv_image_decoder_t* decoder = nullptr;
    if(decoder) return true;
    decoder = lv_image_decoder_create();
    if(!decoder) return false;
    lv_image_decoder_set_info_cb(decoder, Info);
    lv_image_decoder_set_open_cb(decoder, Open);
    lv_image_decoder_set_get_area_cb(decoder, Area);
    lv_image_decoder_set_close_cb(decoder, Close);
    return true;
}
