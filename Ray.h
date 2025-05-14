#pragma once
#include "Vec3.h"
// --- Ray Definition ---
struct Ray {
    Vec3 origin;
    Vec3 direction; // Should be normalized

    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(d.normalize()) {}
};
