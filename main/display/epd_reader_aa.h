#pragma once

// UI-owned reading preference, snapshotted by EpdSubmit for waveform selection.
// Image gray and LVGL blending do not depend on this setting.
bool EpdReaderAaEnabled();
void EpdSetReaderAaEnabled(bool enabled);
