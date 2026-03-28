#include "make_cubemap.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "helperlibs/stb_image_lib/stb_image_write.h"


void rgbe_to_rgb(stbi_uc R, stbi_uc G, stbi_uc B, stbi_uc E, float *newR, float *newG, float *newB) {
    
    *newR = powf(2.0f, float(E) - 128.0f) * ((float(R) + 0.5f) / 256.0f);
	*newG = powf(2.0f, float(E) - 128.0f) * ((float(G) + 0.5f) / 256.0f);
	*newB = powf(2.0f, float(E) - 128.0f) * ((float(B) + 0.5f) / 256.0f);
    
    return;
}

// with help from 15466 code https://github.com/ixchow/15-466-ibl/blob/master/rgbe.hpp
void rgb_to_rgbe(vec3 RGB, stbi_uc *R, stbi_uc *G, stbi_uc *B, stbi_uc *E) {
    int exp;

    float maxColor = max_all(RGB);

    float scale = 256.0f * std::frexp(maxColor, &exp) / maxColor;

    *R = uint8_t(RGB[0] * scale);
    *G = uint8_t(RGB[1] * scale);
    *B = uint8_t(RGB[2] * scale); 
    *E = uint8_t(exp + 128);

    return;
}

vec3 get_normals(float u, float v, int face) {
    switch (face) {
        case 0: return vec3{ 1.0f, -v, -u} / norm(vec3{ 1.0f, -v, -u}); break;
        case 1: return vec3{-1.0f, -v,  u} / norm(vec3{-1.0f, -v,  u}); break;
        case 2: return vec3{ u,  1.0f,  v} / norm(vec3{ u,  1.0f,  v}); break;
        case 3: return vec3{ u, -1.0f, -v} / norm(vec3{ u, -1.0f, -v}); break;
        case 4: return vec3{ u, -v,  1.0f} / norm(vec3{ u, -v,  1.0f}); break;
        case 5: return vec3{-u, -v, -1.0f} / norm(vec3{-u, -v, -1.0f}); break;
    }

    return {0.0f, 0.0f, 0.0f};
}

void output_lambertian_map(std::string src, std::string dst){
    int img_width, img_height, channels;
    // loads in a full 32 bytes 
    
    const int output_img_w = 4;
    const int output_img_h = 4;
    const int faces = 6;
    
    // need output img char *

    stbi_uc *cubemap = stbi_load(src.c_str(), &img_width, &img_height, &channels, 4);
    std::cout << "got the cubemap!" << std::endl;
    std::cout << "img_width: " << img_width << std::endl;
    std::cout << "img_height: " << img_height << std::endl;

    //size_t img_size = img_width * img_height;
   
    std::vector< stbi_uc > outData(output_img_w * output_img_h * faces * 4);

    for (int out_fi = 0; out_fi < faces; ++out_fi) {
        for (int out_hi = 0; out_hi < output_img_h; ++out_hi) {
            for (int out_wi = 0; out_wi < output_img_w; ++out_wi) {
                float outU = ((out_wi + 0.5f) / output_img_w) * 2.0f - 1.0f;
                float outV = ((out_hi + 0.5f) / output_img_h) * 2.0f - 1.0f;

                vec3 normal = get_normals(outU, outV, out_fi);
                float newR, newG, newB;
                vec3 acc = vec3{0.0f, 0.0f, 0.0f};
                float weight = 0.0f;

                for (int in_fi = 0; in_fi < faces; ++in_fi) {
                    for (int in_hi = 0; in_hi < img_height / 6; ++in_hi) {
                        for (int in_wi = 0; in_wi < img_width; ++in_wi) {
                            float u = ((in_wi + 0.5f) / img_width) * 2.0f - 1.0f;
                            float v = ((in_hi + 0.5f) / img_width) * 2.0f - 1.0f;

                            int idx = ((in_fi * img_width + in_hi) * img_width + in_wi) * 4;
                            stbi_uc R = *(cubemap + idx);
                            stbi_uc G = *(cubemap + idx + 1);
                            stbi_uc B = *(cubemap + idx + 2);
                            stbi_uc E = *(cubemap + idx + 3);

                            // std::cout << "R_inner: " << int(R) << std::endl;
                            // std::cout << "G: " << int(G) << std::endl;
                            // std::cout << "B: " << int(B) << std::endl;
                            // std::cout << "E_inner: " << int(E) << std::endl;

                            rgbe_to_rgb(R, G, B, E, &newR, &newG, &newB);

                            vec3 colors = vec3{newR, newG, newB};
                            float solid_angle_approx = 1.0f / powf(1.0f + u*u + v*v, 1.5f);

                            vec3 inc_dir = get_normals(u, v, in_fi);

                            acc += colors * std::max(0.0f, dot(normal, inc_dir)) * solid_angle_approx;
                            weight += std::max(0.0f, dot(normal, inc_dir)) * solid_angle_approx;
                        }
                    }
                }

                acc = acc / weight;

                stbi_uc outR, outG, outB, outE;
                rgb_to_rgbe(acc, &outR, &outG, &outB, &outE); 

                int output_index = ((out_fi * output_img_h + out_hi) * output_img_w + out_wi) * 4;
                // std::cout << "R: " << int(outR) << std::endl;
                // std::cout << "G: " << int(outG) << std::endl;
                // std::cout << "B: " << int(outB) << std::endl;
                // std::cout << "E: " << int(outE) << std::endl;

                outData[output_index] = outR;
                outData[output_index + 1] = outG;
                outData[output_index + 2] = outB;
                outData[output_index + 3] = outE;
            }
        }
    }
    
    stbi_write_png(dst.c_str(), output_img_w, output_img_h * faces, 4, outData.data(), output_img_w * 4);

    stbi_image_free(cubemap);
}

void get_images(int argc, char **argv, std::string *input_png, std::string *output_png) {
    for (int argi = 1; argi < argc; ++argi) { 
        std::string arg = argv[argi];

        if (arg == "--lambertian") {
            argi += 1;
            *output_png = argv[argi];
            std::cout << "here is your output png: " << *output_png << std::endl;
        }

        else if (arg.find(".png") != std::string::npos) {
            *input_png = arg;
            std::cout << "here is your input png: " << *input_png << std::endl;
        }
    }
}