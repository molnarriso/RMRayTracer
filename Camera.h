#pragma once
#include <cmath>
#include <algorithm> // For std::clamp
#include "Vec3.h"
#include "Ray.h"

#ifndef M_PI // Define M_PI if not already defined (e.g., by <cmath> with _USE_MATH_DEFINES)
#define M_PI 3.14159265358979323846
#endif

struct Camera {
    Vec3 position;
    Vec3 look_at_target;
    Vec3 world_up_vector;
    double fov_degrees;

    Vec3 forward_direction;
    Vec3 right_direction;
    Vec3 up_direction;

    double image_plane_dist;
    double image_plane_h;
    double image_plane_w;
    double aspect_ratio;

    double yaw_radians;
    double pitch_radians;

    static constexpr double MIN_PITCH = -M_PI / 2.0 + 0.017; // Approx -89 deg
    static constexpr double MAX_PITCH = M_PI / 2.0 - 0.017; // Approx +89 deg

    Camera(const Vec3& pos = Vec3(0, 1, 3),
        const Vec3& target = Vec3(0, 0, 0),
        const Vec3& world_up = Vec3(0, 1, 0),
        double fov = 60.0)
        : position(pos), look_at_target(target), world_up_vector(world_up.normalize()),
        fov_degrees(fov), yaw_radians(0.0), pitch_radians(0.0), aspect_ratio(0.0),
        image_plane_dist(1.0), image_plane_h(0.0), image_plane_w(0.0) {
    }

    void update_orientation_vectors() {
        forward_direction.x = std::cos(yaw_radians) * std::cos(pitch_radians);
        forward_direction.y = std::sin(pitch_radians);
        forward_direction.z = std::sin(yaw_radians) * std::cos(pitch_radians);
        forward_direction = forward_direction.normalize();

        right_direction = Vec3::cross(forward_direction, world_up_vector).normalize();
        // Handle near-parallel case for forward_direction and world_up_vector (gimbal lock at poles)
        if (right_direction.length_squared() < 1e-6) {
            // If looking straight up/down, derive 'right' from a different axis to prevent issues.
            // Example: if forward is (0,1,0) or (0,-1,0), right could be (cos(yaw), 0, sin(yaw)) based on XZ plane.
            // A simple robust fix is to use a fixed global axis if world_up is too close to forward.
            Vec3 robust_up_alternative = (std::abs(Vec3::dot(forward_direction, Vec3(0, 1, 0))) > 0.999) ? Vec3(1, 0, 0) : world_up_vector;
            if (std::abs(Vec3::dot(forward_direction, Vec3(1, 0, 0))) > 0.999 && robust_up_alternative.x == 1.0) robust_up_alternative = Vec3(0, 0, 1);

            right_direction = Vec3::cross(forward_direction, robust_up_alternative).normalize();
            // Fallback if still zero (highly unlikely if robust_up_alternative is chosen well)
            if (right_direction.length_squared() < 1e-6) {
                right_direction = Vec3(std::cos(yaw_radians + M_PI / 2.0), 0, std::sin(yaw_radians + M_PI / 2.0));
            }
        }
        up_direction = Vec3::cross(right_direction, forward_direction).normalize();

        if (aspect_ratio > 0) {
            // image_plane_dist is often set to 1.0 for simplicity
            double fov_rad = fov_degrees * (M_PI / 180.0);
            image_plane_h = 2.0 * image_plane_dist * std::tan(fov_rad / 2.0);
            image_plane_w = image_plane_h * aspect_ratio;
        }
    }

    void initialize(double screen_aspect_ratio) {
        aspect_ratio = screen_aspect_ratio;
        world_up_vector = world_up_vector.normalize();

        Vec3 initial_forward = (look_at_target - position).normalize();
        yaw_radians = std::atan2(initial_forward.z, initial_forward.x);

        double forward_proj_xz_len = std::sqrt(initial_forward.x * initial_forward.x + initial_forward.z * initial_forward.z);
        // Avoid division by zero if initial_forward is perfectly vertical
        if (forward_proj_xz_len < 1e-6 && initial_forward.y != 0.0) {
            pitch_radians = (initial_forward.y > 0) ? MAX_PITCH : MIN_PITCH;
        }
        else if (forward_proj_xz_len > 1e-6) {
            pitch_radians = std::atan2(initial_forward.y, forward_proj_xz_len);
        }
        else { // initial_forward is (0,0,0) or very small, default pitch
            pitch_radians = 0.0;
        }
        pitch_radians = std::clamp(pitch_radians, MIN_PITCH, MAX_PITCH);

        update_orientation_vectors();
    }

    Ray get_ray(double u_norm, double v_norm) const { // u, v normalized [0,1]
        double u_film = u_norm - 0.5;
        double v_film = 0.5 - v_norm; // Invert v for typical screen coords (y down)

        Vec3 point_on_plane = right_direction * (u_film * image_plane_w) +
            up_direction * (v_film * image_plane_h) +
            forward_direction * image_plane_dist;
        return Ray(position, point_on_plane.normalize());
    }

    void move_forward(double amount) {
        position = position + forward_direction * amount;
    }
    void move_sideways(double amount) { // Positive for right
        position = position + right_direction * amount;
    }
    void move_vertical(double amount) { // Positive for up (along camera's local up)
        position = position + up_direction * amount;
    }

    void rotate_yaw(double angle_rad) { // Positive for CCW from above
        yaw_radians += angle_rad;
        // Keep yaw_radians within [-PI, PI] or [0, 2PI] if desired, though atan2 handles it
        // if (yaw_radians > M_PI) yaw_radians -= 2.0 * M_PI;
        // if (yaw_radians < -M_PI) yaw_radians += 2.0 * M_PI;
    }

    void rotate_pitch(double angle_rad) { // Positive for up
        pitch_radians += angle_rad;
        pitch_radians = std::clamp(pitch_radians, MIN_PITCH, MAX_PITCH);
    }
};