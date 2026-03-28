#version 450 
#define OCTAVES 5
// varying position from vertex shader 
// interpolated so that the value is based on the barycentric coord of the 
// fragment relative to the triangle vertices 
layout(location = 0) in vec2 position;

// declares the color output 
layout(location = 0) out vec4 outColor;

/**
* struct is supplied by CPU via push_constant
* uniform ~ struct
* block name (Push) ~ type name
* instance name (after last brace) ~ name of struct 
**/
layout(push_constant) uniform Push {
    float time;
}; // add instance name here

// Referenced off of https://thebookofshaders.com/13/
// Specifically domain warping, exploring fractal brownian motion with cloud/gas shapes

float random (in vec2 coord) {
    return fract(sin(dot(coord, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noiseGen (in vec2 coord) {
    vec2 i = floor(coord);
    vec2 f = fract(coord);
    
    float c1 = random(i);
    float c2 = random(i + vec2(1.0, 0.0));
    float c3 = random(i + vec2(0.0, 1.0));
    float c4 = random(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(c1, c2, u.x) + 
           (c3 - c1) * u.y * (1.0 - u.x) + 
           (c4 - c2) * u.x * u.y;
}

float gen_fBrownNoise(in vec2 coord) {
    float v = 0.0;
    float a = 0.5;

    vec2 shift = vec2(100.0);

    // mat2 rot = mat2(cos(0.75), -sin(0.75),
    //                 sin(0.75),  cos(0.75));

    // mat2 rot = mat2(cos(0.33), -sin(0.33),
    //                 sin(0.33),  cos(0.33));

    // mat2 rot = mat2(cos(0.5), -sin(0.5),
    //                 sin(0.5),  cos(0.5));

    // interesting graininess... like water
    mat2 rot = mat2(exp(0.5), -sin(0.75),
                    sin(0.75),  exp(0.5));

    // mat2 rot = mat2(exp(0.75), -exp(0.75),
    //                 exp(0.75),  exp(0.75));

    for (int i = 0; i < OCTAVES; ++i) {
        v += a * noiseGen(coord);

        coord = rot * coord * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

void main() {
    // fract similar to modulus 
    // RGBA format 


    //float red = (pow(cos(position.x * 500 / 50), 2) + pow(sin(position.y * 500 / 50), 2) == 100.0f) ? 1.0 : 0.0;
    // float red = position.x / position.y + time;
    
    // float green = (time < 10.0) ? 
    //               cos(position.x * 500 / 50) / time + sin(position.y * 500 / 50) + time :
    //               cos(position.x * 500 / 50) * time + sin(position.y * 500 / 50) + time;

    // // float blue = (time < 10.0) ? 
    // //              cos(position.x * 500 / 50) * time + sin(position.y * 500 / 50) + time : 
    // //              1.0;
    // float blue = 1.0;

    // outColor = vec4( red,
                     
    //                  green,
                     
    //                  blue, 
                     
    //                  1.0 );


    // outColor = vec4(fract(position.x + time), fract(position.y + time), 0.0, 1.0);
    // outColor = vec4(0.0, fract(position.y), fract(position.x), 0.5);

    outColor = vec4(0.0, 0.0, 0.0, 1.0);
    // outColor = vec4(

    // )


    //how zoomed in you are into the noise
    // vec2 coord = gl_FragCoord.xy / vec2(1600.0, 800.0) * 3.0;
    // vec3 color = vec3(0.0);

    // vec2 q;
    // q.x = gen_fBrownNoise(coord + 0.0 * time);
    // q.y = gen_fBrownNoise(coord + vec2(1.0));

    // vec2 r;
    // r.x = gen_fBrownNoise(coord + 1.0 * q + vec2(1.7, 9.2) + 0.15 * time);
    // r.y = gen_fBrownNoise(coord + 1.0 * q + vec2(8.3, 2.8) + 0.126 * time);

    // vec2 s;
    // s.x = gen_fBrownNoise(coord + 1.0 * r + vec2(2.5, 8.2) + 0.2 * time);
    // s.y = gen_fBrownNoise(coord + 1.0 * r + vec2(9.6, 3.0) + 0.14 * time);

    // float f = gen_fBrownNoise(coord + r + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + r + gen_fBrownNoise(coord + s))));

    // //float f = gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s + gen_fBrownNoise(coord + s))))))))))));

    // color = mix(vec3(0.101961, 0.619608, 0.666667),
    //             vec3(0.666667, 0.666667, 0.498039),
    //             clamp((f * f) * 4.0, 0.0, 1.0));

    // color = mix(color, 
    //             vec3(0, 0.6, 0.164706), 
    //             clamp(length(q), 0.0, 1.0));

    // color = mix(color, 
    //             vec3(0.666667, 1, 1), 
    //             clamp(length(r.x), 0.0, 1.0));
    
    // color = mix(color, 
    //             vec3(0.666667, 0.494290, 0.1138), 
    //             clamp(length(q), 0.0, 1.0));

    // color = mix(color, 
    //             vec3(0.01, 0.0, 0.5555), 
    //             clamp(length(r.x) / 2, 0.0, 1.0));
    
    // outColor = vec4((pow(f, 3) + 0.6 * pow(f, 2) + 0.5 * pow(f, 1)) * color * sin(color) / cos(color),
    //                 1.0);
        
}

