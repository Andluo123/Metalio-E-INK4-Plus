#include "settings_frontlight_tab.h"

#include "assets/lang_config.h"
#include "assets/lang_screen.h"
#include "fontpack_lvgl.h"
#include "frontlight.h"
#include "epd_refresh.h"
#include "haptic_feedback.h"
#include "settings.h"
#include "settings_common.h"

#include <cstdio>
#include <algorithm>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "SettingsFl"

namespace {

constexpr const char* kNvsNs = "frontlight";
constexpr const char* kNvsKeyCct = "cct";

struct SettingsFrontlightOption {
    FrontlightCct cct; // 色温组合
    const char* (*title)(); // 选项标题
    const char* (*current_text)(); // 当前选中文案
};

static const char* TitleCool() {
    return Lang::Strings::SETTINGS_FRONTLIGHT_COOL;
}
static const char* TitleWarm() {
    return Lang::Strings::SETTINGS_FRONTLIGHT_WARM;
}
static const char* TitleBoth() {
    return Lang::Strings::SETTINGS_FRONTLIGHT_BOTH;
}
static const char* TitleOff() {
    return Lang::Strings::SETTINGS_FRONTLIGHT_OFF;
}
static const char* CurCool() {
    return Lang::Strings::SETTINGS_FRONTLIGHT_CURRENT_COOL;
}
static const char* CurWarm() {
    return Lang::Strings::SETTINGS_FRONTLIGHT_CURRENT_WARM;
}
static const char* CurBoth() {
    return Lang::Strings::SETTINGS_FRONTLIGHT_CURRENT_BOTH;
}
static const char* CurOff() {
    return Lang::Strings::SETTINGS_FRONTLIGHT_CURRENT_OFF;
}

static constexpr SettingsFrontlightOption kOptions[] = {
    {FrontlightCct::kCool, TitleCool, CurCool},
    {FrontlightCct::kWarm, TitleWarm, CurWarm},
    {FrontlightCct::kBoth, TitleBoth, CurBoth},
    {FrontlightCct::kOff, TitleOff, CurOff},
};
static constexpr int kOptionCount = sizeof(kOptions) / sizeof(kOptions[0]);

struct SettingsFrontlightUi {
    lv_obj_t* profiles[4] = {};
    lv_obj_t* refresh_hint = nullptr;
    lv_obj_t* depths[3] = {};
    lv_obj_t* depth_hint = nullptr;
    lv_obj_t* brightness_slider = nullptr;
    lv_obj_t* btns[kOptionCount] = {}; // 色温选项
    lv_obj_t* current_lbl = nullptr; // 当前色温文案
    lv_obj_t* brightness_lbl = nullptr; // 亮度文案
};

static SettingsFrontlightUi s_ui;

static FrontlightCct NormalizeCct(int value) {
    if (value < static_cast<int>(FrontlightCct::kWarm) ||
        value > static_cast<int>(FrontlightCct::kOff)) {
        return FrontlightCct::kOff;
    }
    return static_cast<FrontlightCct>(value);
}

static FrontlightCct CurrentCct() {
    auto& fl = Frontlight::GetInstance();
    if (!fl.initialized()) {
        return FrontlightCct::kOff;
    }
    return fl.cct();
}

static const char* CurrentText(FrontlightCct cct) {
    for (int i = 0; i < kOptionCount; ++i) {
        if (kOptions[i].cct == cct) {
            return kOptions[i].current_text();
        }
    }
    return kOptions[0].current_text();
}

static uint8_t CurrentBrightness() {
    auto& fl = Frontlight::GetInstance();
    if (!fl.initialized()) {
        return 0;
    }
    return fl.target_brightness();
}

static void RefreshBrightnessLabel() {
    if (s_ui.brightness_lbl == nullptr) {
        return;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), Lang::Strings::SETTINGS_FRONTLIGHT_BRIGHTNESS_FMT,
                  static_cast<int>(CurrentBrightness()));
    lv_label_set_text(s_ui.brightness_lbl, buf);
}

static void RefreshOptions() {
    const auto profile = EpdGetRefreshProfile();
    for (int i=0;i<4;++i) {
        if (s_ui.profiles[i]) SettingsStyleSelectable(s_ui.profiles[i],
            lv_obj_get_child(s_ui.profiles[i],0),static_cast<int>(profile)==i);
    }
    if (s_ui.refresh_hint != nullptr) {
        const char* hints[]={Lang::Strings::SETTINGS_SCREEN_REFRESH_CLEAR_HINT,
            Lang::Strings::SETTINGS_SCREEN_REFRESH_BALANCED_HINT,Lang::Strings::SETTINGS_SCREEN_REFRESH_FAST_HINT,
            Lang::Strings::SETTINGS_SCREEN_REFRESH_EXTREME_HINT};
        lv_label_set_text(s_ui.refresh_hint,hints[static_cast<int>(profile)]);
    }
    const int depth=static_cast<int>(EpdGetBlackDepth());
    for (int i=0;i<3;++i) {
        if (s_ui.depths[i]) SettingsStyleSelectable(s_ui.depths[i],lv_obj_get_child(s_ui.depths[i],0),depth==i);
    }
    if (s_ui.depth_hint) {
        if (depth==0) lv_obj_add_flag(s_ui.depth_hint,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(s_ui.depth_hint,LV_OBJ_FLAG_HIDDEN);
    }
    const FrontlightCct cct = CurrentCct();
    for (int i = 0; i < kOptionCount; ++i) {
        if (s_ui.btns[i] != nullptr) {
            SettingsStyleSelectable(s_ui.btns[i], lv_obj_get_child(s_ui.btns[i], 0),
                                    kOptions[i].cct == cct);
        }
    }
    if (s_ui.current_lbl != nullptr) {
        lv_label_set_text(s_ui.current_lbl, CurrentText(cct));
    }
    RefreshBrightnessLabel();
}

static void OnDepthClicked(lv_event_t* e) {
    const auto depth=static_cast<EpdBlackDepth>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (EpdSetBlackDepth(depth)) RefreshOptions();
}
static lv_obj_t* OptionRow(lv_obj_t* parent) {
    auto* row=lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row,lv_pct(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row,UiSx(12),0);
    lv_obj_clear_flag(row,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row,LV_OBJ_FLAG_CLICKABLE);
    return row;
}
static void FitRowButton(lv_obj_t* button) {
    lv_obj_set_width(button,0);
    lv_obj_set_flex_grow(button,1);
}

static void OnProfileClicked(lv_event_t* e) {
    const auto profile=static_cast<EpdRefreshProfile>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    EpdSetRefreshProfile(profile);
    RefreshOptions();
}

static void PersistCctTask(void* arg) {
    const int cct = static_cast<int>(reinterpret_cast<intptr_t>(arg));
    Settings settings(kNvsNs, true);
    settings.SetInt(kNvsKeyCct, cct);
    ESP_LOGI(TAG, "nvs save cct=%d", cct);
    vTaskDelete(nullptr);
}

static void ApplyCct(FrontlightCct cct) {
    auto& fl = Frontlight::GetInstance();
    if (!fl.initialized()) {
        ESP_LOGW(TAG, "frontlight not ready");
        return;
    }
    fl.SetCct(cct);
    const int value = static_cast<int>(cct);
    if (xTaskCreate(PersistCctTask, "fl_cct_nvs", 4096,
                    reinterpret_cast<void*>(static_cast<intptr_t>(value)), 5, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate(fl_cct_nvs) failed");
    }
}

static void OnOptionClicked(lv_event_t* e) {
    const auto cct = NormalizeCct(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));
    if (CurrentCct() == cct) {
        return;
    }
    ApplyCct(cct);
    RefreshOptions();
}

static void PersistBrightnessTask(void* arg) {
    {
        Settings settings(kNvsNs,true);
        settings.SetInt("brightness",static_cast<int>(reinterpret_cast<intptr_t>(arg)));
    }
    vTaskDelete(nullptr);
}

static void OnBrightnessSlider(lv_event_t* e) {
    auto& fl=Frontlight::GetInstance();
    if(!fl.initialized())return;
    const int value=lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(e)));
    fl.SetBrightness(static_cast<uint8_t>(value),false);
    RefreshBrightnessLabel();
    // Persist only on release, away from the LVGL task.
    if(lv_event_get_code(e)==LV_EVENT_RELEASED &&
       xTaskCreate(PersistBrightnessTask,"fl_bri_nvs",4096,
           reinterpret_cast<void*>(static_cast<intptr_t>(value)),5,nullptr)!=pdPASS)
        ESP_LOGW(TAG,"brightness persist task failed");
}

static void OnBrightnessStep(lv_event_t* e) {
    auto& fl = Frontlight::GetInstance();
    if (!fl.initialized()) return;
    const auto code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
        const int delta = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
        const int value = std::clamp<int>(fl.target_brightness() + delta, 0, 100);
        fl.SetBrightness(static_cast<uint8_t>(value), false);
        lv_slider_set_value(s_ui.brightness_slider, value, LV_ANIM_OFF);
        RefreshBrightnessLabel();
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (xTaskCreate(PersistBrightnessTask, "fl_bri_nvs", 4096,
            reinterpret_cast<void*>(static_cast<intptr_t>(fl.target_brightness())), 5, nullptr) != pdPASS)
            ESP_LOGW(TAG, "brightness persist task failed");
    }
}

} // namespace

void SettingsFrontlightTab_Refresh() {
    RefreshOptions();
    if (s_ui.brightness_slider != nullptr)
        lv_slider_set_value(s_ui.brightness_slider, CurrentBrightness(), LV_ANIM_OFF);
}

void SettingsFrontlightTab_Reset() {
    s_ui = {};
}

void SettingsFrontlightTab_Build(lv_obj_t* page) {
    s_ui = {};

    auto* refresh_title=lv_label_create(page);
    lv_label_set_text(refresh_title,Lang::Strings::SETTINGS_SCREEN_REFRESH_TITLE);
    lv_obj_set_style_text_font(refresh_title,fontpack_lv_font_ui(),0);
    lv_obj_set_style_text_color(refresh_title,lv_color_black(),0);
    lv_obj_t* profile_row=nullptr;
    const char* profile_names[]={Lang::Strings::SETTINGS_SCREEN_REFRESH_CLEAR,
        Lang::Strings::SETTINGS_SCREEN_REFRESH_BALANCED,Lang::Strings::SETTINGS_SCREEN_REFRESH_FAST,
        Lang::Strings::SETTINGS_SCREEN_REFRESH_EXTREME};
    for (int i=0;i<4;++i) {
        if (i%2==0) profile_row=OptionRow(page);
        s_ui.profiles[i]=SettingsCreateSelectableOption(profile_row,profile_names[i],OnProfileClicked,i);
    }
    for (auto* button:s_ui.profiles) FitRowButton(button);
    s_ui.refresh_hint = lv_label_create(page);
    lv_label_set_text(s_ui.refresh_hint, Lang::Strings::SETTINGS_SCREEN_REFRESH_FAST_HINT);
    lv_obj_set_width(s_ui.refresh_hint, lv_pct(100));
    lv_label_set_long_mode(s_ui.refresh_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_ui.refresh_hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_ui.refresh_hint, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.refresh_hint, LV_OBJ_FLAG_CLICKABLE);
    auto* refresh_separator=lv_obj_create(page);
    lv_obj_remove_style_all(refresh_separator);
    lv_obj_set_size(refresh_separator,lv_pct(100),kSettingsSplitLineW);
    lv_obj_set_style_bg_color(refresh_separator,lv_color_black(),0);
    lv_obj_set_style_bg_opa(refresh_separator,LV_OPA_COVER,0);
    lv_obj_clear_flag(refresh_separator,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(refresh_separator,LV_OBJ_FLAG_SCROLLABLE);
    auto* depth_title=lv_label_create(page);
    lv_label_set_text(depth_title,Lang::Strings::SETTINGS_SCREEN_BLACK_DEPTH_TITLE);
    lv_obj_set_style_text_font(depth_title,fontpack_lv_font_ui(),0);
    lv_obj_set_style_text_color(depth_title,lv_color_black(),0);
    auto* depth_row=OptionRow(page);
    const char* depth_names[]={Lang::Strings::SETTINGS_SCREEN_BLACK_DEPTH_DEFAULT,
        Lang::Strings::SETTINGS_SCREEN_BLACK_DEPTH_MEDIUM,Lang::Strings::SETTINGS_SCREEN_BLACK_DEPTH_STRONG};
    for (int i=0;i<3;++i) {
        s_ui.depths[i]=SettingsCreateSelectableOption(depth_row,depth_names[i],OnDepthClicked,i);
        FitRowButton(s_ui.depths[i]);
    }
    s_ui.depth_hint=lv_label_create(page);
    lv_label_set_text(s_ui.depth_hint,Lang::Strings::SETTINGS_SCREEN_BLACK_DEPTH_HINT);
    lv_obj_set_width(s_ui.depth_hint,lv_pct(100));
    lv_label_set_long_mode(s_ui.depth_hint,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_ui.depth_hint,fontpack_lv_font_ui(),0);
    lv_obj_set_style_text_color(s_ui.depth_hint,lv_color_black(),0);
    lv_obj_clear_flag(s_ui.depth_hint,LV_OBJ_FLAG_CLICKABLE);
    auto* separator=lv_obj_create(page);
    lv_obj_remove_style_all(separator);
    lv_obj_set_size(separator,lv_pct(100),kSettingsSplitLineW);
    lv_obj_set_style_bg_color(separator,lv_color_black(),0);
    lv_obj_set_style_bg_opa(separator,LV_OPA_COVER,0);
    lv_obj_clear_flag(separator,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(separator,LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(page);
    lv_label_set_text(title, Lang::Strings::SETTINGS_FRONTLIGHT_TITLE);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* hint = lv_label_create(page);
    lv_label_set_text(hint, Lang::Strings::SETTINGS_FRONTLIGHT_HINT);
    lv_obj_set_style_text_font(hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(hint, lv_color_black(), 0);
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* cct_row=nullptr;
    for (int i = 0; i < kOptionCount; ++i) {
        if (i%2==0) cct_row=OptionRow(page);
        s_ui.btns[i] = SettingsCreateSelectableOption(
            cct_row, kOptions[i].title(), OnOptionClicked,
            static_cast<intptr_t>(static_cast<int>(kOptions[i].cct)));
        FitRowButton(s_ui.btns[i]);
    }

    s_ui.current_lbl = lv_label_create(page);
    lv_label_set_text(s_ui.current_lbl, "");
    lv_obj_set_style_text_font(s_ui.current_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_ui.current_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.current_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* bright_row = lv_obj_create(page);
    lv_obj_remove_style_all(bright_row);
    lv_obj_set_width(bright_row, lv_pct(100));
    lv_obj_set_height(bright_row, kSettingsOptionH);
    lv_obj_clear_flag(bright_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bright_row, LV_OBJ_FLAG_CLICKABLE);

    const lv_coord_t edge = kSettingsBorderW;
    auto make_step = [&](const char* text, intptr_t delta) {
        auto* button = SettingsCreateSelectableOption(bright_row, text, OnBrightnessStep, delta);
        SettingsStyleBrightnessButton(button);
        lv_obj_align(button, delta < 0 ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID,
                     delta < 0 ? edge : -edge, 0);
        for (auto code : {LV_EVENT_PRESSED, LV_EVENT_LONG_PRESSED_REPEAT, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
            lv_obj_add_event_cb(button, OnBrightnessStep, code, reinterpret_cast<void*>(delta));
    };
    make_step("-", -1);

    auto* slider=lv_slider_create(bright_row);
    s_ui.brightness_slider = slider;
    lv_obj_update_layout(bright_row);
    const lv_coord_t slider_start = edge + kSettingsBrightnessButtonW + kSettingsBrightnessSliderGap;
    lv_obj_set_size(slider,lv_obj_get_content_width(bright_row) - 2 * slider_start,UiSy(16));
    lv_obj_align(slider, LV_ALIGN_LEFT_MID, slider_start, 0);
    lv_slider_set_range(slider,0,100);
    lv_slider_set_value(slider,CurrentBrightness(),LV_ANIM_OFF);
    lv_obj_set_style_radius(slider,LV_RADIUS_CIRCLE,LV_PART_MAIN);
    lv_obj_set_style_radius(slider,LV_RADIUS_CIRCLE,LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider,lv_color_white(),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(slider,1,LV_PART_MAIN);
    lv_obj_set_style_border_color(slider,lv_color_black(),LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider,lv_color_black(),LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider,LV_OPA_COVER,LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider,lv_color_white(),LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider,LV_OPA_COVER,LV_PART_KNOB);
    lv_obj_set_style_border_width(slider,2,LV_PART_KNOB);
    lv_obj_set_style_border_color(slider,lv_color_black(),LV_PART_KNOB);
    lv_obj_set_style_radius(slider,LV_RADIUS_CIRCLE,LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider,std::max<int>(0,(UiSx(30)-UiSy(16))/2),LV_PART_KNOB);
    lv_obj_add_event_cb(slider,OnBrightnessSlider,LV_EVENT_VALUE_CHANGED,nullptr);
    lv_obj_add_event_cb(slider,OnBrightnessSlider,LV_EVENT_RELEASED,nullptr);
    make_step("+", 1);

    auto* controls = lv_obj_create(page);
    lv_obj_remove_style_all(controls);
    lv_obj_set_size(controls, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(controls, LV_OBJ_FLAG_CLICKABLE);
    s_ui.brightness_lbl = lv_label_create(controls);
    lv_label_set_text(s_ui.brightness_lbl, "");
    lv_obj_set_style_text_font(s_ui.brightness_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_ui.brightness_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.brightness_lbl, LV_OBJ_FLAG_CLICKABLE);

    RefreshOptions();
}
