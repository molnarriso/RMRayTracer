#pragma once
#include <cmath>
#include "Vec3.h"
#include "Ray.h"
#include <cstdint> // For uint32_t
#include <algorithm> // For std::clamp
#include "Material.h"
#include <random>    // For random number generation

struct SphereData { // Using a temporary struct for loading
    Vec3 center;
    double radius;
    std::string material_id_ref; // ID of the material to use

    SphereData(const Vec3& c, double r, const std::string& mat_id)
        : center(c), radius(r), material_id_ref(mat_id) {
    }
};

// Helper function to generate a random unit vector in a hemisphere around a normal
// This is a simple cosine-weighted hemisphere sampling.
// A more uniform random point in a sphere, then normalized and checked if in hemisphere,
// could also work for a very basic approach.
inline Vec3 random_in_hemisphere(const Vec3& normal, std::mt19937& rng) {
    std::uniform_real_distribution<double> dist01(0.0, 1.0);
    while (true) {
        // Generate a random point in a unit cube
        double x = dist01(rng) * 2.0 - 1.0;
        double y = dist01(rng) * 2.0 - 1.0;
        double z = dist01(rng) * 2.0 - 1.0;
        Vec3 p(x, y, z);
        if (p.length_squared() >= 1.0) continue; // Reject points outside unit sphere
        p = p.normalize();
        if (Vec3::dot(p, normal) > 0.0) { // Ensure it's in the hemisphere defined by the normal
            return p;
        }
        // Optional: could also return -p if it's in the other hemisphere,
        // but for reflection, we want it in the normal's hemisphere.
        // This simple rejection sampling works but isn't the most efficient for perfect cosine weighting.
    }
}
// A more common (and often better for BRDFs) cosine-weighted hemisphere sampling:
inline Vec3 random_cosine_direction(std::mt19937& rng) {
    std::uniform_real_distribution<double> dist01(0.0, 1.0);
    double r1 = dist01(rng);
    double r2 = dist01(rng);
    double z = std::sqrt(1.0 - r2);
    double phi = 2.0 * M_PI * r1;
    double x = std::cos(phi) * std::sqrt(r2);
    double y = std::sin(phi) * std::sqrt(r2);
    return Vec3(x, y, z);
}

// Function to create an orthonormal basis (ONB) from a normal vector
inline void create_onb(const Vec3& normal, Vec3& tangent, Vec3& bitangent) {
    if (std::abs(normal.x) > std::abs(normal.y)) {
        tangent = Vec3(-normal.z, 0, normal.x) / std::sqrt(normal.x * normal.x + normal.z * normal.z);
    }
    else {
        tangent = Vec3(0, normal.z, -normal.y) / std::sqrt(normal.y * normal.y + normal.z * normal.z);
    }
    bitangent = Vec3::cross(normal, tangent);
}

struct Sphere {
    Vec3 center;
    double radius;
    Material material;

    Sphere(const Vec3& c, double r, const Material& mat)
        : center(c), radius(r), material(mat) {
    }

    bool intersect(const Ray& ray, double& t_out, Vec3& hit_point_out, Vec3& normal_at_hit_out) const {
        Vec3 oc = ray.origin - center; // Vector from sphere center to ray origin

        // These coefficients are for the quadratic equation: a*t^2 + b*t + c = 0
        // where t is the distance along the ray.
        // a = ray.direction . ray.direction (dot product)
        // b = 2 * (ray.origin - sphere.center) . ray.direction
        // c = (ray.origin - sphere.center) . (ray.origin - sphere.center) - sphere.radius^2

        double a = Vec3::dot(ray.direction, ray.direction); // Should be 1.0 if ray.direction is normalized,
        // but calculating it makes it robust even if not.
        double b = 2.0 * Vec3::dot(oc, ray.direction);
        double c = Vec3::dot(oc, oc) - radius * radius;

        double discriminant = b * b - 4.0 * a * c;

        if (discriminant < 0.0) {
            return false; // No real roots, so the ray misses the sphere
        }
        else {
            // Calculate the two potential intersection distances (t values)
            double sqrt_discriminant = std::sqrt(discriminant);
            double t0 = (-b - sqrt_discriminant) / (2.0 * a);
            double t1 = (-b + sqrt_discriminant) / (2.0 * a);

            // We need the smallest positive t value, as that's the closest intersection
            // in front of the ray's origin.
            // Using a small epsilon (t_min) to avoid self-intersections or hits that are too close.
            const double intersection_t_min = 1e-4; // Minimum distance for a valid hit

            double t_hit = -1.0; // Sentinel for no valid hit found yet

            if (t0 > intersection_t_min) { // If t0 is a valid hit in front
                t_hit = t0;
            }
            // If t0 was not valid or t1 is closer (and also valid)
            if (t1 > intersection_t_min && (t_hit < 0.0 || t1 < t_hit)) {
                t_hit = t1;
            }

            if (t_hit > 0.0) { // If a valid t_hit was found
                t_out = t_hit;
                hit_point_out = ray.origin + ray.direction * t_hit;
                normal_at_hit_out = (hit_point_out - center).normalize();
                return true;
            }
            else {
                return false; // Both intersections are behind or too close to the origin
            }
        }
    }
};