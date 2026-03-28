#pragma once

// A small matrix math library for 4x4 matrices

#include <array>
#include <cmath>
#include <cstdint>
#include "vec3.hpp"
#include <iostream>
#include <glm/glm/mat4x4.hpp>

// COLUMN MAJOR order!
using mat4 = std::array< float, 16 >;
// using mat4 = glm::mat4;

static_assert(sizeof(mat4) == 16 * 4, "mat4 is exactly 16 32-bit floats");

using vec4 = std::array < float, 4 >;
static_assert(sizeof(vec4) == 4 * 4, "vec4 is exactly 4 32-bit floats");

inline vec4 operator-(vec4 const &a, vec4 const &b) {

    // compute ret = A * b;

    return vec4{a[0] - b[0], 
                a[1] - b[1],
                a[2] - b[2],
                a[3] - b[3]};
}

inline vec4 operator+(vec4 const &a, vec4 const &b) {
    // compute ret = A * b;

    return vec4{a[0] + b[0], 
                a[1] + b[1],
                a[2] + b[2],
                a[3] + b[3]};
}

inline vec4 operator*(vec4 const &a, float const b) {
    // compute ret = A * b;

    return vec4{a[0] * b, 
                a[1] * b,
                a[2] * b,
                a[3] * b};
}

inline vec4 operator*(float const b, vec4 const &a) {
    // compute ret = A * b;

    return vec4{a[0] * b, 
                a[1] * b,
                a[2] * b,
                a[3] * b};
}

inline float norm_squared(const vec4 &a) {
    return a[0] * a[0] + a[1] * a[1] + a[2] * a[2] + a[3] * a[3];
}

inline float norm(const vec4 &a) {
    return std::sqrt(norm_squared(a));
}

inline vec4 operator*(mat4 const &A, vec4 const &b) {
    vec4 ret;

    // compute ret = A * b;
    for (uint32_t r = 0; r < 4; ++r) {
        ret[r] = A[0 * 4 + r] * b[0];
        for (uint32_t k = 1; k < 4; ++k) {
            ret[r] += A[k * 4 + r] * b[k];
        }
    }

    return ret;
}

inline mat4 identity_4x4() {
    return mat4{1.0f, 0.0f, 0.0f, 0.0f, 
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f};
}

inline mat4 operator*(mat4 const &A, mat4 const &B) {
    mat4 ret;

    for (uint32_t c = 0; c < 4; ++c) {
        for (uint32_t r = 0; r < 4; ++r) {
            ret[c * 4 + r] = A[0 * 4 + r] * B[c * 4 + 0];
            for (uint32_t k = 1; k < 4; ++k) {
                ret[c * 4 + r] += A[k * 4 + r] * B[c * 4 + k];
            }
        }
    }

    return ret;
}

inline void print_matrix4x4(mat4 M) {
    std::cout << M[0] << ", " << M[1] << ", " << M[2] << ", " << M[3] << "\n" 
              << M[4] << ", " << M[5] << ", " << M[6] << ", " << M[7] << "\n"  
              << M[8] << ", " << M[9] << ", " << M[10] << ", " << M[11] << "\n" 
              << M[12] << ", " << M[13] << ", " << M[14] << ", " << M[15] << "\n" << std::endl;          
}

inline void print_vector_4x1(vec4 v) {
    std::cout << v[0] << ", " << v[1] << ", " << v[2] << ", " << v[3] << std::endl;
}

/**
 * perspective proj matrix
 * - vfov is fov in radians
 * - near maps to 0, far maps to 1
 * looks down -z with +y up and +x right
 */
inline mat4 perspective(float vfov, float aspect, float near, float far) {
    // vulkan device coords are y-down
    // rescale z

    const float e = 1.0f / std::tan(vfov / 2.0f);
    const float a = aspect;
    const float n = near;
    const float f = far;

    return mat4{
        e/a,  0.0f,                            0.0f, 0.0f,
        0.0f,   -e,                            0.0f, 0.0f,
        0.0f, 0.0f, -0.5f - 0.5f * (f + n)/(f - n), -1.0f,
        0.0f, 0.0f,               -(f * n)/(f - n),  0.0f,
    };
}

/**
 * makes camera_space_from_world matrix for a camera at eye looking toward  
 * target with up-vector pointing as close as possible along up   
 */ 
inline mat4 look_at (vec3 &eye, vec3 &target, vec3 &up) {
    // compute vector from eye to target
    vec3 in = vec3{0,0,0};

    for (size_t i = 0; i < 3; ++i) {
        in[i] = target[i] - eye[i];
    }

    // normalize in vector
    float inv_in_norm = 1.0f / std::sqrt(norm_squared(in));
    in *= inv_in_norm;

    // make up orthogonal to in
    float in_dot_up = dot(in, up);
    up -= in * in_dot_up;

    // normalize up 
    float inv_up_norm = 1.0f / std::sqrt(norm_squared(up));
    up *= inv_up_norm;

    // compute right = in x up
    vec3 right = cross(in, up);

    float right_dot_eye = dot(right, eye);
    float up_dot_eye = dot(up, eye);
    float in_dot_eye = dot(in, eye);

    // final matrix: dot(right, v - eye), dot(up, v - eye), -in . (v - eye), dot(v, w)
    return (mat4{ // column major order!
        right[0], up[0], -in[0], 0.0f,
        right[1], up[1], -in[1], 0.0f,
        right[2], up[2], -in[2], 0.0f,
        -right_dot_eye, -up_dot_eye, in_dot_eye, 1.0f
    });
}

/**
 * orbit camera matrix:
 * 
 * makes a camera-from-world matrix fora  camera orbiting target_{x, y, z} 
 * at distance radius with angles azimuth and elevation
 * 
 * azimuth (rad) is CCW angle in the xy plane from the x axis
 * elevation (rad) is angle up from the xy plane
 */
inline mat4 orbit(vec3 target, float azimuth, float elevation, float radius) {
    float ca = std::cos(azimuth);
    float sa = std::sin(azimuth);
    float ce = std::cos(elevation);
    float se = std::sin(elevation);

    // camera's right direction = azimuth rotated by 90 degrees
    vec3 right = vec3{-sa, ca, 0.0f};

    // camera's up dir is elevation rotated 90 degrees and points in same xy dir as azimuth
    vec3 up = vec3{-se * ca, -se * sa, ce};

    // direction to camera from the target
    vec3 out = vec3{ce * ca, ce * sa, se};

    // camera's position
    vec3 eye = target + radius * out;

    // camera's position projected on the various vectors
    float right_dot_eye = dot(right, eye);
    float up_dot_eye = dot(up, eye);
    float out_dot_eye = dot(out, eye);

    // final local-from-world transformation (column-major)
    return mat4 {
        right[0], up[0], out[0], 0.0f,
        right[1], up[1], out[1], 0.0f,
        right[2], up[2], out[2], 0.0f,
        -right_dot_eye, -up_dot_eye, -out_dot_eye, 1.0f,
    };
}

inline vec3 get_eye(vec3 target, float azimuth, float elevation, float radius) {
    float ca = std::cos(azimuth);
    float sa = std::sin(azimuth);
    float ce = std::cos(elevation);
    float se = std::sin(elevation);

    // std::cout << "azimuth: " << azimuth << std::endl;
    // std::cout << "elevation: " << elevation << std::endl;

    // direction to camera from the target
    vec3 out = vec3{ce * ca, ce * sa, se};

    // camera's position
    vec3 eye = target + radius * out;

    return eye;
}

// from https://www.songho.ca/opengl/gl_quaternion.html#final
inline mat4 get_rotation_matrix(vec4 unit_quaternion) {
    float x = unit_quaternion[0];
    float y = unit_quaternion[1];
    float z = unit_quaternion[2];
    float s = unit_quaternion[3];

    float diag_0 = 1.0f - 2.0f*powf(y, 2.0f) - 2*powf(z, 2.0f);
    float diag_1 = 1.0f - 2.0f*powf(x, 2.0f) - 2*powf(z, 2.0f);
    float diag_2 = 1.0f - 2.0f*powf(x, 2.0f) - 2*powf(y, 2.0f);

    // in column major order, 
    return mat4{
        diag_0       , 2*x*y + 2*s*z, 2*x*z - 2*s*y, 0,
        2*x*y - 2*s*z, diag_1       , 2*y*z + 2*s*x, 0,
        2*x*z + 2*s*y, 2*y*z - 2*s*x, diag_2       , 0,
                    0,             0,             0, 1
    };
}

inline mat4 get_translation_matrix(vec3 translate_vec) {
    float tx = translate_vec[0];
    float ty = translate_vec[1];
    float tz = translate_vec[2];

    return mat4{
        1,   0,  0, 0,
        0,   1,  0, 0,
        0,   0,  1, 0,
        tx, ty, tz, 1
    };
}

inline mat4 get_scale_matrix(vec3 scale_vec) {
    float sx = scale_vec[0];
    float sy = scale_vec[1];
    float sz = scale_vec[2];

    return mat4{
        sx, 0,  0, 0,
        0, sy,  0, 0, 
        0,  0, sz, 0,
        0,  0,  0, 1
    };
}

inline mat4 get_parent_from_local(mat4 transMat, mat4 rotMat, mat4 scaleMat) {
    return transMat * rotMat * scaleMat;
}

inline glm::mat4 convert_to_glm_mat4(mat4 const M) {
    return glm::mat4(M[0], M[1], M[2], M[3],
                     M[4], M[5], M[6], M[7],
                     M[8], M[9], M[10], M[11],
                     M[12], M[13], M[14], M[15]);
}

inline mat4 convert_back_to_mymat4(glm::mat4 M) {
    return mat4{M[0][0], M[0][1], M[0][2], M[0][3],
                M[1][0], M[1][1], M[1][2], M[1][3],
                M[2][0], M[2][1], M[2][2], M[2][3],
                M[3][0], M[3][1], M[3][2], M[3][3]};       
}

inline float dot(vec4 const &u, vec4 const &v) {
    return u[0] * v[0] + u[1] * v[1] + u[2] * v[2] + u[3] * v[3];
}

// with help from matlab resources
inline vec4 invert_quat(vec4 quat) {
    float q0 = quat[3];
    float q1 = quat[0];
    float q2 = quat[1];
    float q3 = quat[2];

    float mag_quat = norm_squared(quat);

    return vec4{-q1 / mag_quat, -q2 / mag_quat, -q3 / mag_quat, q0 / mag_quat};

}

// with help from matlab resources
inline vec4 mult_quats(vec4 quat0, vec4 quat1) {
    float q0 = quat0[3];
    float q1 = quat0[0];
    float q2 = quat0[1];
    float q3 = quat0[2];

    float r0 = quat1[3];
    float r1 = quat1[0];
    float r2 = quat1[1];
    float r3 = quat1[2];

    return vec4{r0 * q1 + r1 * q0 - r2 * q3 + r3 * q2,
                r0 * q2 + r1 * q3 + r2 * q0 - r3 * q1,
                r0 * q3 - r1 * q2 + r2 * q1 + r3 * q0, 
                r0 * q0 - r1 * q1 - r2 * q2 - r3 * q3};
}


inline vec4 quat_exp(vec4 quat) {
    float q0 = quat[3];
    float q1 = quat[0];
    float q2 = quat[1];
    float q3 = quat[2];

    float e_a = exp(q0);
    vec3 IJKcomps = vec3{q1, q2, q3};
    float normQuat = norm(quat);
    float normIJK = norm(IJKcomps);

     vec3 sign_vec = (normQuat != 0) ? (IJKcomps / normIJK) : vec3{0.0f, 0.0f, 0.0f};

    vec4 ret;

    ret[3] = e_a * cos(normIJK);
    ret[0] = e_a * (sign_vec[0]) * sin(normIJK);
    ret[1] = e_a * (sign_vec[1]) * sin(normIJK);
    ret[2] = e_a * (sign_vec[2]) * sin(normIJK);

    return ret;
}


// https://math.stackexchange.com/questions/939229/unit-quaternion-to-a-scalar-power
inline vec4 quat_power(vec4 &quat, float power) {
    float q0 = quat[3];
    float q1 = quat[0];
    float q2 = quat[1];
    float q3 = quat[2];

    vec3 IJKcomps = vec3{q1, q2, q3};
    float normQuat = norm(quat);
    float normIJK = norm(IJKcomps);

    vec3 sign_vec = (normQuat != 0) ? (IJKcomps / normIJK) : vec3{0.0f, 0.0f, 0.0f};

    vec4 ln_q = {sign_vec[0] * acos(q0 / normQuat),
                 sign_vec[1] * acos(q0 / normQuat),
                 sign_vec[2] * acos(q0 / normQuat), 
                 log(normQuat)};

    return quat_exp(power * ln_q);
}
