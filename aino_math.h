// =====================================================
// aino_math.hpp - 数学基础库
// =====================================================

#pragma once
#include <array>
#include <cmath>
#include <immintrin.h>

namespace aino_math {

// 3D向量
struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    
    Vec3 operator+(const Vec3& v) const { return {x+v.x, y+v.y, z+v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x-v.x, y-v.y, z-v.z}; }
    Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    Vec3& operator+=(const Vec3& v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
};

// 叉积 (正确顺序：a × b)
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// 点积
inline float dot(const Vec3& a, const Vec3& b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

// 四元数
struct alignas(16) Quaternion {
    float x, y, z, w;
    static Quaternion from_euler(float roll, float pitch, float yaw) {
        float cr = std::cos(roll * 0.5f), sr = std::sin(roll * 0.5f);
        float cp = std::cos(pitch * 0.5f), sp = std::sin(pitch * 0.5f);
        float cy = std::cos(yaw * 0.5f), sy = std::sin(yaw * 0.5f);
        return {
            sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy
        };
    }
};

// SIMD包装
namespace simd {
    inline __m128 load(const float* p) { return _mm_load_ps(p); }
    inline void store(float* p, __m128 v) { _mm_store_ps(p, v); }
    inline __m128 noise4() { 
        // 简化噪声生成
        return _mm_set_ps(rand()/(float)RAND_MAX, rand()/(float)RAND_MAX, 
                          rand()/(float)RAND_MAX, rand()/(float)RAND_MAX);
    }
}

} // namespace aino_math
