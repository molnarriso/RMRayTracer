#pragma once
#include "Vec3.h"
#include <algorithm> // For std::clamp
#include <cstdint>   // For uint32_t, uint8_t

// Converts a 32-bit integer color (0xAARRGGBB) to a Vec3 (normalized 0-1 components).
// Assumes StretchDIBits expects BGRA in memory for a uint32_t on little-endian systems.
inline Vec3 uint32_to_vec3_color(uint32_t color_int) {
    uint8_t r = (color_int >> 16) & 0xFF;
    uint8_t g = (color_int >> 8) & 0xFF;
    uint8_t b = color_int & 0xFF;
    return Vec3(static_cast<double>(r) / 255.0,
        static_cast<double>(g) / 255.0,
        static_cast<double>(b) / 255.0);
}

// Converts a Vec3 color (normalized 0-1 components) to a 32-bit integer (0xAARRGGBB).
inline uint32_t vec3_to_uint32_color(const Vec3& color_vec) {
    uint8_t r = static_cast<uint8_t>(std::clamp(color_vec.x, 0.0, 1.0) * 255.0);
    uint8_t g = static_cast<uint8_t>(std::clamp(color_vec.y, 0.0, 1.0) * 255.0);
    uint8_t b = static_cast<uint8_t>(std::clamp(color_vec.z, 0.0, 1.0) * 255.0);
    uint8_t alpha = 0xFF; // Opaque alpha
    return (alpha << 24) | (r << 16) | (g << 8) | b;
}

// Note: The global BACKGROUND_COLOR_UINT32 and BACKGROUND_COLOR_VEC3 constants
// might be superseded if the background color is loaded from a scene file.
// If they are intended as fallbacks or defaults, their placement here is fine.
// const uint32_t DEFAULT_BACKGROUND_COLOR_UINT32 = 0xFF202020;
// const Vec3 DEFAULT_BACKGROUND_COLOR_VEC3 = uint32_to_vec3_color(DEFAULT_BACKGROUND_COLOR_UINT32);