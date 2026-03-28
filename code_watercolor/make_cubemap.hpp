#include "Tutorial.hpp"
#include "helperlibs/mathlibs/mat4.hpp"
#include "helperlibs/S72_loader/S72.hpp"
#include "helperlibs/timer.hpp"
#include "helperlibs/rgb_decoders.hpp"
#include "helperlibs/stb_image_lib/stb_image_write.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>

void output_lambertian_map(std::string src, std::string dst);

void get_images(int argc, char **argv, std::string *input_png, std::string *output_png);

void rgbe_to_rgb(stbi_uc R, stbi_uc G, stbi_uc B, stbi_uc E, 
                            float *newR, float *newG, float *newB);

void rgb_to_rgbe(vec3 RGB, stbi_uc *R, stbi_uc *G, stbi_uc *B, stbi_uc *E);

vec3 get_normals(float u, float v, int face);
