//
// Created by Sawyer on 2/11/2025.
//

#ifndef OPENXR_GLES_APP_VRGAME_H
#define OPENXR_GLES_APP_VRGAME_H

#include <cmath>  // Needed for sqrt

// -------- Vec3 Struct & Math Functions --------
struct Vec3 {
    float x, y, z;
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};


// Addition
inline Vec3 operator+(const Vec3& a, const Vec3& b) {
    return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

// Subtraction
inline Vec3 operator-(const Vec3& a, const Vec3& b) {
    return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

// Scalar multiplication
inline Vec3 operator*(const Vec3& v, float s) {
    return Vec3(v.x * s, v.y * s, v.z * s);
}

inline Vec3 operator*(float s, const Vec3& v) {
    return v * s;
}

// Scalar division
inline Vec3 operator/(const Vec3& v, float s) {
    return Vec3(v.x / s, v.y / s, v.z / s);
}

// Compound operators
inline Vec3& operator+=(Vec3& a, const Vec3& b) {
    a.x += b.x; a.y += b.y; a.z += b.z;
    return a;
}

inline Vec3& operator-=(Vec3& a, const Vec3& b) {
    a.x -= b.x; a.y -= b.y; a.z -= b.z;
    return a;
}

inline Vec3& operator*=(Vec3& v, float s) {
    v.x *= s; v.y *= s; v.z *= s;
    return v;
}

inline Vec3& operator/=(Vec3& v, float s) {
    v.x /= s; v.y /= s; v.z /= s;
    return v;
}

// Unary negation
inline Vec3 operator-(const Vec3& v) {
    return Vec3(-v.x, -v.y, -v.z);
}

// Dot product
inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Cross product
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
    );
}

// Vector length
inline float length(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

// Vector normalization
inline Vec3 normalize(const Vec3& v) {
    float len = length(v);
    return len != 0.0f ? v / len : Vec3(0, 0, 0);
}

// Distance between two points
inline float distance(const Vec3& a, const Vec3& b) {
    return length(b - a);
}



int init_Sphere ();
int draw_Sphere (Vec3 position, Vec3 col, float *matP, float *matV);

#endif //OPENXR_GLES_APP_VRGAME_H



