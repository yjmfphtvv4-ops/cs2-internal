#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>

struct Vector2 {
    float x, y;

    Vector2() : x(0.f), y(0.f) {}
    Vector2(float x, float y) : x(x), y(y) {}

    Vector2 operator+(const Vector2& v) const { return { x + v.x, y + v.y }; }
    Vector2 operator-(const Vector2& v) const { return { x - v.x, y - v.y }; }
    Vector2 operator*(float s) const { return { x * s, y * s }; }
};

struct Vector3 {
    float x, y, z;

    Vector3() : x(0.f), y(0.f), z(0.f) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vector3 operator+(const Vector3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vector3 operator-(const Vector3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    Vector3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vector3 operator/(float s) const { return { x / s, y / s, z / s }; }
    Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float Dot(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    float LengthSqr() const { return x * x + y * y + z * z; }
    float Length2D() const { return std::sqrt(x * x + y * y); }
    Vector3 Cross(const Vector3& v) const { return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x }; }
    bool IsZero() const { return x == 0.f && y == 0.f && z == 0.f; }
    bool IsValid() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }

    Vector3 Normalized() const {
        float len = Length();
        if (len < 0.0001f) return *this;
        return *this / len;
    }
};

struct view_matrix_t {
    float data[16];

    float* operator[](int i) { return &data[i * 4]; }
    const float* operator[](int i) const { return &data[i * 4]; }

    Vector3 Transform(const Vector3& v) const {
        float x = data[0] * v.x + data[1] * v.y + data[2] * v.z + data[3];
        float y = data[4] * v.x + data[5] * v.y + data[6] * v.z + data[7];
        float z = data[8] * v.x + data[9] * v.y + data[10] * v.z + data[11];
        float w = data[12] * v.x + data[13] * v.y + data[14] * v.z + data[15];
        if (w < 0.001f) return {};
        float invW = 1.f / w;
        return { x * invW, y * invW, z * invW };
    }
};

struct Vector4 {
    float x, y, z, w;
};

struct QAngle {
    float pitch, yaw, roll;

    QAngle() : pitch(0.f), yaw(0.f), roll(0.f) {}
    QAngle(float p, float y, float r) : pitch(p), yaw(y), roll(r) {}

    QAngle operator+(const QAngle& v) const { return { pitch + v.pitch, yaw + v.yaw, roll + v.roll }; }
    QAngle operator-(const QAngle& v) const { return { pitch - v.pitch, yaw - v.yaw, roll - v.roll }; }
    QAngle operator/(float s) const { return { pitch / s, yaw / s, roll / s }; }

    void Clamp() {
        pitch = std::clamp(pitch, -89.0f, 89.0f);
        yaw = std::fmod(yaw, 360.0f);
        if (yaw > 180.0f) yaw -= 360.0f;
        if (yaw < -180.0f) yaw += 360.0f;
        roll = std::clamp(roll, -50.0f, 50.0f);
    }
};

static bool WorldToScreen(const Vector3& world, Vector2& screen, const view_matrix_t& vm, int sw, int sh) {
    auto transformed = vm.Transform(world);
    if (transformed.z < 0.f || transformed.z > 1.f) return false;

    screen.x = (sw / 2.0f) + (transformed.x * sw / 2.0f);
    screen.y = (sh / 2.0f) - (transformed.y * sh / 2.0f);
    return true;
}

static QAngle CalcAngle(const Vector3& src, const Vector3& dst) {
    Vector3 delta = dst - src;
    float length = delta.Length();

    QAngle angles;
    angles.pitch = -asinf(delta.z / length) * (180.0f / 3.1415926535f);
    angles.yaw = atan2f(delta.y, delta.x) * (180.0f / 3.1415926535f);
    angles.roll = 0.f;
    return angles;
}
