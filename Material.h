#pragma once
// Material.h
#pragma once
#include <cstdint>   // For uint32_t
#include <algorithm> // For std::clamp
#include "Vec3.h"

struct Material {
    std::string id;         // Unique identifier for the material
    Vec3 base_color;        // R,G,B from 0.0 to 1.0
    double reflectivity;
    double roughness;
    Vec3 emission_color;    // Color emitted by this material

    Material(
        const std::string& material_id = "default",
        const Vec3& color = Vec3(0.8, 0.8, 0.8),
        double refl = 0.0,
        double rough = 0.0,
        const Vec3& emission = Vec3(0.0, 0.0, 0.0) // Default no emission
    ) : id(material_id),
        base_color(color),
        reflectivity(std::clamp(refl, 0.0, 1.0)),
        roughness(std::clamp(rough, 0.0, 1.0)),
        emission_color(emission) {
    }
};