#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include "vec3.hpp"

// derived from https://www.shadertoy.com/view/4lscWj

float get_radInv_vdc(uint32_t currNum) {
    currNum = (currNum << 16u) | (currNum >> 16u);
    currNum = ((currNum & 0x55555555u) << 1u) | ((currNum & 0xAAAAAAAAu) >> 1u);
    currNum = ((currNum & 0x33333333u) << 2u) | ((currNum & 0xCCCCCCCCu) >> 2u); 
    currNum = ((currNum & 0x0F0F0F0Fu) << 4u) | ((currNum & 0xF0F0F0F0u) >> 4u); 
    currNum = ((currNum & 0x00FF00FFu) << 8u) | ((currNum & 0xFF00FF00u) >> 8u); 

    return float(currNum) * 2.3283064365386963e-10;
}

void hammersley_sampler(float curr_sample, float num_samples, float *x, float *y) {
    // do the radical inverse
    uint32_t sample = static_cast<uint32_t>(curr_sample);
    
    *x = curr_sample / num_samples;
    *y = get_radInv_vdc(sample);
}

// From Karis' notes, https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
vec3 ggx_imp_sample(float x, float radInv, float roughness, vec3 normal) {
    float alpha = roughness * roughness;

    float phi = 2.0f * M_PI * x;
    float cos_t = sqrtf((1.0f - radInv) / (radInv* (alpha*alpha - 1.0f) + 1.0f));
    float sin_t = sqrtf(1.0f - cos_t * cos_t);

    vec3 halfedge = vec3{sin_t * cos(phi), sin_t * sin(phi), cos_t};

    vec3 upDir = abs(normal[2] < 0.999) ? vec3{0, 0, 1} : vec3{1, 0, 0};
    vec3 tangent_x = normalize(cross(upDir, normal));
    vec3 tangent_y = cross(normal, tangent_x);

    return tangent_x * halfedge[0] + tangent_y * halfedge[1] + normal * halfedge[2];
}

float get_geo_attenuation(float roughness, float ndotv, float ndotl) {
    
    float k = ((roughness + 1) * (roughness + 1)) / 8;
    float g1_l = ndotl / ((ndotl) * (1 - k) + k);
    float g1_v = ndotv / ((ndotv) * (1 - k) + k);

    return g1_l * g1_v;
}

void integrate_brdf(float roughness, float ndotv, float *scale, float *bias) {
    vec3 viewer = vec3{sqrtf(1.0f - ndotv * ndotv),
                       0.0f,
                       ndotv};

    float left = 0;
    float right = 0;
    vec3 normal = vec3{0, 0, 1};
    
    const uint32_t tot_samples = 1024;
    for (uint32_t i; i < tot_samples; ++i) {
        float x, y;
        
        hammersley_sampler(i, tot_samples, &x, &y);
        vec3 half_vector = ggx_imp_sample(x, y, roughness, normal);
        vec3 light = 2.0f * dot(viewer, half_vector) * half_vector - viewer;

        float ndotl = std::clamp(light[2], 0.0f, 1.0f);
        float ndoth = std::clamp(half_vector[2], 0.0f, 1.0f);
        float vdoth = std::clamp(dot(viewer, half_vector), 0.0f, 1.0f);

        if (ndotl > 0.0f) {
            float geo = get_geo_attenuation(roughness, ndotv, ndotl); 

            float visibility = (geo * vdoth) / (ndoth * ndotv);
            float coeff = powf(1.0f - vdoth, 5.0f);
            left += (1 - coeff) * visibility;
            right += coeff * visibility;
        }
    }

    *scale = left / tot_samples;
    *bias = right / tot_samples;
}

// scale is the R16, bias is the G16








