#pragma once
#include <lvgl.h>

// Presentation intent, independent of source format and page names. Only tagged
// visible content opts into gray. Everything else, including icons, is binary.
enum class EpdContentRole {
    Monochrome, FastMonochrome, Shelf, Cover, Illustration, WallpaperThumbnail,
    WallpaperPreview, Photo, ReaderAntialias, AssistantImage
};
// FastMonochrome forces its whole subtree to binary without changing saved images.
void EpdSetContentRole(lv_obj_t* object, EpdContentRole role);
bool EpdVisibleContentNeedsGray(lv_display_t* display);
bool EpdContentBlendInit();
