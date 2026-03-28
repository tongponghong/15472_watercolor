#pragma once

// A small vector math library for vec3s based on Scotty3D

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

using vec3 = std::array < float, 3 >;

// struct vec3 {
//     union {
//         float data_array[3];
        
//         struct {
//             float x, y, z;
//         };

//         struct {
//             float r, g, b;
//         };
//     };

//     float& operator[](uint32_t index) {
//         return data_array[index];
//     }

//     float operator[](uint32_t index) const {
//         return data_array[index];
//     }
// };

static_assert(sizeof(vec3) == 3 * 4, "vec4 is exactly 3 32-bit floats");


inline float norm_squared(const vec3 &a) {
    return a[0] * a[0] + a[1] * a[1] + a[2] * a[2];
}

inline float norm(const vec3 &a) {
    return std::sqrt(norm_squared(a));
}

inline float dot(vec3 const &u, vec3 const &v) {
    return u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
}

inline vec3 cross(vec3 const &u, vec3 const &v) {
    return vec3{u[1] * v[2] - u[2] * v[1],
                u[2] * v[0] - u[0] * v[2],
                u[0] * v[1] - u[1] * v[0]};
}

inline vec3 operator-(vec3 u, vec3 v) {
    vec3 ret;

    for (size_t r = 0; r < 3; ++r) {
        ret[r] = u[r] - v[r];
    }
    
    return ret;
}

inline vec3 operator+(vec3 const &u, vec3 const &v) {
    vec3 ret;

    for (size_t r = 0; r < 3; ++r) {
        ret[r] = u[r] + v[r];
    }
    
    return ret;
}

inline vec3 operator*(vec3 const &u, float b) {
    return vec3{u[0] * b, u[1] * b, u[2] * b};
}

inline vec3 operator*(float b, vec3 const &u) {
    return vec3{u[0] * b, u[1] * b, u[2] * b};
}

inline vec3 operator/(vec3 const &u, float b) {
    return vec3{u[0] / b, u[1] / b, u[2] / b};
}
inline vec3 operator-=(vec3 &a, float b) {
    a[0] -= b;
    a[1] -= b;
    a[2] -= b;

    return a;
}
inline vec3 operator+=(vec3 &a, float b) {
    a[0] += b;
    a[1] += b;
    a[2] += b;

    return a;
}

inline vec3 operator-=(vec3 &a, vec3 const &b) {
    a[0] -= b[0];
    a[1] -= b[1];
    a[2] -= b[2];

    return a;
}
inline vec3 operator+=(vec3 &a, vec3 const &b) {
    a[0] += b[0];
    a[1] += b[1];
    a[2] += b[2];

    return a;
}

inline vec3 operator*=(vec3 &a, float b) {
    a[0] *= b;
    a[1] *= b;
    a[2] *= b;

    return a;
}

inline bool operator<(vec3 &a, vec3 &b) {
    return ((a[0] < b[0]) && 
            (a[1] < b[1]) &&
            (a[2] < b[2]));
}

inline void print_vector_3x1(vec3 v) {
    std::cout << v[0] << ", " << v[1] << ", " << v[2] << std::endl;
}

inline vec3 min_comps(vec3 const v1, vec3 const v2) {
    return vec3{std::min(v1[0], v2[0]), 
                std::min(v1[1], v2[1]), 
                std::min(v1[2], v2[2])};
}

inline vec3 max_comps(vec3 const v1, vec3 const v2) {

    return vec3{std::max(v1[0], v2[0]), 
                std::max(v1[1], v2[1]), 
                std::max(v1[2], v2[2])};
}

inline float max_all(vec3 v1) {
    return std::max(std::max(v1[1], v1[2]), v1[0]);
}

inline vec3 normalize(const vec3 &a) {
    return a / norm(a);
}