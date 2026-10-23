#pragma once

bool SettingsLogUpload_IsActive();
bool SettingsLogUpload_IsRunning();
void SettingsLogUpload_Start(); // LVGL thread only
void SettingsLogUpload_Teardown(); // LVGL thread only
void SettingsLogUpload_RequestAbort(const char* why); // any task, nonblocking
bool SettingsLogUpload_OnVkKey(const char* key); // LVGL thread only
