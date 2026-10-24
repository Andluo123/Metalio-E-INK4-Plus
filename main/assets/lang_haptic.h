#pragma once

// Extra haptic strings live here because lang_config.h is now generated at build
// time from lang_home.cc, so ad-hoc declarations do not belong in it.
namespace Lang::Strings {
extern const char* SETTINGS_HAPTIC_INTENSITY_TITLE;
extern const char* SETTINGS_HAPTIC_WEAK;
extern const char* SETTINGS_HAPTIC_MEDIUM;
extern const char* SETTINGS_HAPTIC_STRONG;
extern const char* SETTINGS_HAPTIC_CURRENT_WEAK;
extern const char* SETTINGS_HAPTIC_CURRENT_MEDIUM;
extern const char* SETTINGS_HAPTIC_CURRENT_STRONG;
}
