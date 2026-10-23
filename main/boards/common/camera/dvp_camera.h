#pragma once

#include <cstdint>
#include <vector>
#include <esp_err.h>

// Local factory-test API. Does not register a Camera/MCP/assistant endpoint.
namespace dvp_camera {
struct Request {
    uint32_t timeout_ms = 8000;
    bool (*cancelled)(void*) = nullptr;
    void* context = nullptr;
};
struct DetectInfo {
    uint16_t pid = 0;
};
struct GrayImage {
    uint16_t width = 0;
    uint16_t height = 0;
    std::vector<uint8_t> pixels;
};
esp_err_t Detect(DetectInfo& info, const Request& request);
// Return the configured VGA frame at its native resolution.
esp_err_t Capture(GrayImage& image, const Request& request);
}
