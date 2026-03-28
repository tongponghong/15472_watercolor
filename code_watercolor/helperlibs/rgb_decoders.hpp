#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <bit>



typedef unsigned char uc;
// https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf#section*.33

inline void rgb9_e5_from_rgbe8(float red, float green, float blue, uint32_t *rgbe) {
    float N = 9.0f;
    float B = 15.0f;
    float Emax = 31.0f;

    float shared_max_exp = float(((pow(2, N) - 1) / pow(2, N)) * pow(2, Emax - B));
    
    float clamped_red = std::max(0.0f, std::min(shared_max_exp, float(red)));
    float clamped_green = std::max(0.0f, std::min(shared_max_exp, float(green)));
    float clamped_blue = std::max(0.0f, std::min(shared_max_exp, float(blue)));
    
    float max_comp = std::max(clamped_red, std::max(clamped_green, clamped_blue));

    float prelim_exp = std::max(float(-B-1), std::floor(log2(max_comp))) + 1.0f + B;

    float max_s = std::floor((max_comp / (pow(2, prelim_exp - B - N))) + 0.5f);

    float ref_shared_exp = 0.0f;

    if (0 <= max_s && max_s < powf(2.0f, N)) ref_shared_exp = prelim_exp;
    else if (max_s == powf(2.0f, N)) ref_shared_exp = prelim_exp + 1.0f;
    
    uint32_t red_final = uint32_t(
        std::floor(float(clamped_red / (pow(2, ref_shared_exp - B - N))) + 0.5f));
    uint32_t green_final = uint32_t(
        std::floor(float(clamped_green / (pow(2, ref_shared_exp - B - N))) + 0.5f));
    uint32_t blue_final = uint32_t(
        std::floor(float(clamped_blue / (pow(2, ref_shared_exp - B - N))) + 0.5f));
    
    uint32_t exp_final = 0x1F & uint32_t(ref_shared_exp);
    red_final = 0x1FF & red_final;
    green_final = 0x1FF & green_final;
    blue_final = 0x1FF & blue_final;

    *rgbe = exp_final << 27 | blue_final << 18 | green_final << 9 | red_final;
}