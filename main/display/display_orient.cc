#include "display_orient.h"

#include "config.h"

#include <cstdlib>
#include <atomic>

#include <esp_log.h>
#include <lvgl.h>

namespace {

constexpr const char* TAG = "DispOrient";

std::atomic<int> s_orient = kDisplayUiPortrait;
bool s_full_screen = false;
int s_panel_w = 1216; // ED047 物理宽（横置）
int s_panel_h = 684;  // ED047 物理高
lv_display_t* s_disp = nullptr;

int ClampOrient(int orient) {
    if (orient < 0 || orient >= kDisplayUiOrientCount) {
        return kDisplayUiPortrait;
    }
    return orient;
}

void LogicalToPanelAt(int orient, int lx, int ly, int* px, int* py) {
    const int pw = s_panel_w;
    const int ph = s_panel_h;
    const int left = s_full_screen ? 0 : DISPLAY_CONTENT_LEFT_INSET;
    const int top = s_full_screen ? 0 : DISPLAY_CONTENT_TOP_INSET;
    switch (ClampOrient(orient)) {
        case kDisplayUiLandLeft:
            *px = lx + top;
            *py = ly + left;
            break;
        case kDisplayUiLandRight:
            *px = pw - 1 - lx - top;
            *py = ph - 1 - ly - left;
            break;
        case kDisplayUiPortraitDown:
            *px = pw - 1 - ly - top;
            *py = lx + left;
            break;
        case kDisplayUiPortrait:
        default:
            *px = ly + top;
            *py = ph - 1 - lx - left;
            break;
    }
}

void TouchToLogicalAt(int orient, int tx, int ty, int* lx, int* ly) {
    const int pw = s_panel_w;
    const int ph = s_panel_h;
    const int left = s_full_screen ? 0 : DISPLAY_CONTENT_LEFT_INSET;
    const int top = s_full_screen ? 0 : DISPLAY_CONTENT_TOP_INSET;
    switch (ClampOrient(orient)) {
        case kDisplayUiLandLeft:
            *lx = ty - top;
            *ly = ph - 1 - tx - left;
            break;
        case kDisplayUiLandRight:
            *lx = pw - 1 - ty - top;
            *ly = tx - left;
            break;
        case kDisplayUiPortraitDown:
            *lx = ph - 1 - tx - left;
            *ly = pw - 1 - ty - top;
            break;
        case kDisplayUiPortrait:
        default:
            *lx = tx - left;
            *ly = ty - top;
            break;
    }
}

void FailCheck(const char* what) {
    ESP_LOGE(TAG, "self-check failed: %s", what);
    abort();
}

void SelfCheck() {
    for (int orient = 0; orient < kDisplayUiOrientCount; ++orient) {
        const int w = (orient == kDisplayUiPortrait || orient == kDisplayUiPortraitDown) ? DISPLAY_CONTENT_W : DISPLAY_CONTENT_H;
        const int h = (orient == kDisplayUiPortrait || orient == kDisplayUiPortraitDown) ? DISPLAY_CONTENT_H : DISPLAY_CONTENT_W;
        for (int x = 0; x < w; x += w - 1) {
            for (int y = 0; y < h; y += h - 1) {
                int px, py, lx, ly;
                LogicalToPanelAt(orient, x, y, &px, &py);
                TouchToLogicalAt(orient, s_panel_h - 1 - py, px, &lx, &ly);
                if (lx != x || ly != y || px < 0 || px >= s_panel_w || py < 0 || py >= s_panel_h) {
                    FailCheck("orientation round-trip");
                }
            }
        }
    }
}

}  // namespace

void DisplayUiBind(lv_display_t* disp, int panel_w, int panel_h) {
    s_disp = disp;
    if (panel_w > 0 && panel_h > 0) {
        s_panel_w = panel_w;
        s_panel_h = panel_h;
    }
    s_orient = kDisplayUiPortrait;
    s_full_screen = false;
    SelfCheck();
}

bool DisplayUiSetOrient(int orient) {
    orient = ClampOrient(orient);
    const bool changed = (s_orient != orient);
    s_orient = orient;
    const int log_w = DisplayUiLogW();
    const int log_h = DisplayUiLogH();
    if (s_disp != nullptr) {
        if (lv_display_get_horizontal_resolution(s_disp) != log_w ||
            lv_display_get_vertical_resolution(s_disp) != log_h) {
            lv_display_set_resolution(s_disp, log_w, log_h);
        }
    }
    if (changed) {
        ESP_LOGI(TAG, "orient=%d log=%dx%d (content inset L%d R%d T%d B%d)", orient, log_w, log_h,
                 DISPLAY_CONTENT_LEFT_INSET, DISPLAY_CONTENT_RIGHT_INSET, DISPLAY_CONTENT_TOP_INSET,
                 DISPLAY_CONTENT_BOTTOM_INSET);
    }
    return changed;
}

int DisplayUiGetOrient(void) {
    return s_orient;
}

void DisplayUiSetFullScreen(bool full_screen) {
    if (s_full_screen == full_screen) return;
    s_full_screen = full_screen;
    DisplayUiSetOrient(s_orient);
}

bool DisplayUiIsLandscape(void) {
    return s_orient == kDisplayUiLandLeft || s_orient == kDisplayUiLandRight;
}

int DisplayUiLogW(void) {
    const int w = s_full_screen ? s_panel_h : DISPLAY_CONTENT_W;
    const int h = s_full_screen ? s_panel_w : DISPLAY_CONTENT_H;
    return DisplayUiIsLandscape() ? h : w;
}

int DisplayUiLogH(void) {
    const int w = s_full_screen ? s_panel_h : DISPLAY_CONTENT_W;
    const int h = s_full_screen ? s_panel_w : DISPLAY_CONTENT_H;
    return DisplayUiIsLandscape() ? w : h;
}

void DisplayUiLogicalToPanel(int lx, int ly, int* px, int* py) {
    if (px == nullptr || py == nullptr) {
        return;
    }
    LogicalToPanelAt(s_orient, lx, ly, px, py);
}

void DisplayUiTouchToLogical(int tx, int ty, int* lx, int* ly) {
    if (lx == nullptr || ly == nullptr) {
        return;
    }
    TouchToLogicalAt(s_orient, tx, ty, lx, ly);
}
