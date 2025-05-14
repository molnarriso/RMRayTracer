#pragma once

struct Vec3 {
    double x, y, z;

    Vec3(double x_ = 0.0, double y_ = 0.0, double z_ = 0.0) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(double scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
    Vec3 operator/(double scalar) const { return Vec3(x / scalar, y / scalar, z / scalar); }

    double length_squared() const { return x * x + y * y + z * z; }
    double length() const { return std::sqrt(length_squared()); }

    Vec3 normalize() const {
        double l = length();
        if (l > 1e-8) { // Avoid division by zero
            return Vec3(x / l, y / l, z / l);
        }
        return Vec3(0, 0, 0); // Or handle error
    }

    static double dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return Vec3(a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
    }
};