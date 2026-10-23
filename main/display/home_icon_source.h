#pragma once

// Static Flash-backed pixels when available; otherwise retain the original path.
const void* HomeIconSource(const char* path);
bool HomeIconDecoderReady();
