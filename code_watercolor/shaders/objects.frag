#version 450 
#define PI 3.1415926

struct Sun {
    vec3 SUN_DIRECTION;
    vec3 SUN_ENERGY;
    vec4 angle_w_pad;
};

layout (set = 0, binding = 0, std140) readonly buffer Suns {
    Sun SUN_LIGHTS[];
};

struct SphereLight {
    vec4 SPHERE_POSITION_RADIUS;
    vec4 SPHERE_POWER_LIMIT;
};

layout (set = 2, binding = 0, std140) readonly buffer Spheres {
    SphereLight SPHERE_LIGHTS[];
};

struct SpotLight {
    vec4 SPOT_POSITION_RADIUS;
    vec3 SPOT_DIRECTION;
    vec4 SPOT_POWER_LIMIT;
    vec4 SPOT_OUTER_INNER_LIM;
};

layout (set = 3, binding = 0, std140) readonly buffer Spots {
    SpotLight SPOT_LIGHTS[];
};

struct Transform {
    mat4 CLIP_FROM_LOCAL;
    mat4 WORLD_FROM_LOCAL;
    mat4 WORLD_FROM_LOCAL_NORMAL;
};

layout(push_constant) uniform Push {
    vec3 EYE;
    int tex_type;
    // kept as a push constant to change exposure while debugging 
    float exposure; 
    float time;
}; // add instance name here

layout (constant_id = 0) const int sunNum = 0;
layout (constant_id = 1) const int sphereNum = 0;
layout (constant_id = 2) const int spotNum = 0;
layout (constant_id = 3) const int tonemap_type = 0;

layout (set = 4, binding = 0) uniform sampler2D TEXTURE;
layout (set = 4, binding = 1) uniform samplerCube CUBE_TEXTURE;
layout (set = 4, binding = 2) uniform samplerCube LAMB_SAMPLER; 
layout (set = 4, binding = 3) uniform sampler2D NORMAL_SAMPLER;

// from vertex shader 
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 new_tangent;
layout (location = 3) in vec3 bitangent;
layout (location = 4) in vec2 texCoord;
layout (location = 5) in mat3 TBN_basis;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 controlColor;

//https://64.github.io/tonemapping/

vec3 exposure_scale(vec3 rgb, float exposure) {
    return pow(2.0, exposure) * rgb;
}

float get_luminance(vec3 rgb) {
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

//https://www.ryanjuckett.com/rgb-color-space-conversion/
vec3 linear_to_srgb(vec3 linear_col) {
    vec3 outCol = vec3(0.0);

    for (int i = 0; i < 3; i++) {
        if (linear_col[i] <= 0.0031308) {
            outCol[i] = linear_col[i] * 12.92;
        } 
        else {
            outCol[i] = 1.055 * pow(linear_col[i], 1.0 / 2.4) - 0.055;
        }
    }

    return outCol;
}

vec3 srgb_to_linear(vec3 srgb_col) {
    vec3 outCol = vec3(0.0);

    for (int i = 0; i < 3; i++) {
        if (srgb_col[i] <= 0.04045) {
            outCol[i] = srgb_col[i] / 12.92;
        } 
        else {
            outCol[i] = pow((srgb_col[i] + 0.055) / 1.055, 2.4);
        }
    }

    return outCol;
}

float random2(vec2 coord) {
    return fract(sin(dot(coord, vec2(12.9898, 78.233))) * 43758.5453123);
}

vec2 random22(vec2 p){
   return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3))))*43758.5453);
}


float random3(vec3 coord) {
    float r1 = random2(coord.xy);
    float r2 = random2(coord.yz);
    float r3 = random2(coord.xz);
    return (r1 + r2 + r3).x;
}

vec3 random33(vec3 p){
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.xxy + p.yzz) * p.zyx);
}

float random0(float x) {
   return fract(sin(x * 12.9898 + 78.233) * 43758.5453);
}



//https://iquilezles.org/articles/morenoise/
float noiseGen (in vec3 coord) {
    vec3 i = floor(coord);
    vec3 f = fract(coord);
    
    float r0 = random3(i + vec3(0.0, 0.0, 0.0));
    float r1 = random3(i + vec3(0.0, 0.0, 1.0));
    float r2 = random3(i + vec3(0.0, 1.0, 0.0));
    float r3 = random3(i + vec3(0.0, 1.0, 1.0));
    float r4 = random3(i + vec3(1.0, 0.0, 0.0));
    float r5 = random3(i + vec3(1.0, 0.0, 1.0));
    float r6 = random3(i + vec3(1.0, 1.0, 0.0));
    float r7 = random3(i + vec3(1.0, 1.0, 1.0));

    // quintic interpolation
    vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float c0 = r0;
    float c1 = r1 - r0;
    float c2 = r2 - r0;
    float c3 = r5 - r0;
    float c4 = r0 - r1 - r2 + r3;
    float c5 = r0 - r2 - r4 + r6;
    float c6 = r0 - r1 - r4 + r5;
    float c7 = -r0 + r1 + r2 -r3 + r4 - r5 - r6 + r7;

    // return mix(c1, c2, u.x) + 
    //        (c3 - c1) * u.y * (1.0 - u.x) + 
    //        (c4 - c2) * u.x * u.y;

    return c0 + c1 * u.x + c2 * u.y + c3 * u.z + 
                c4 * u.x * u.y +
                c5 * u.y * u.z + 
                c6 * u.z * u.x +
                c7 * u.x * u.y * u.z;
}

float gen_fBrownNoise(in vec3 coord) {
    float v = 0.0;
    float a = 0.5;
    int OCTAVES = 3;
    // mat2 rot = mat2(cos(0.75), -sin(0.75),
    //                 sin(0.75),  cos(0.75));

    // mat2 rot = mat2(cos(0.33), -sin(0.33),
    //                 sin(0.33),  cos(0.33));

    // mat2 rot = mat2(cos(0.5), -sin(0.5),
    //                 sin(0.5),  cos(0.5));

    // interesting graininess... like water
    // mat2 rot = mat2(exp(0.5), -sin(0.75),
    //                 sin(0.75),  exp(0.5));

    // mat2 rot = mat2(exp(0.75), -exp(0.75),
    //                 exp(0.75),  exp(0.75));

    for (int i = 0; i < OCTAVES; ++i) {
        v += a * noiseGen(coord);
        coord = coord * 1.2;
        a *= 0.5;
    }

    return v;
}



// technically should be based on max luminance but need to figure out how to find that without 193048394 cpu calcs 
vec3 reinhard(vec3 rgb, float white_point) {
    float l_prev = get_luminance(rgb);
    float scale = (1.0 + l_prev / (white_point * white_point));
    float l_new = (l_prev / (1.0 + l_prev)) * scale;

    return rgb * (l_new / l_prev);
}

vec3 tonemapper(vec3 rgb, int tone_type) {
    switch(tone_type) {
        case 0: return clamp(rgb / 1.5, 0.0, 1.0);
        case 1: return reinhard(rgb, 2.0);
    }
}

vec3 get_sun_contribution_lamb(vec3 norm) {
    vec3 e = vec3(0.0);
    for (int i = 0; i < sunNum; ++i) {
        Sun currSun = SUN_LIGHTS[i];
        float ndotl = dot(norm, normalize(currSun.SUN_DIRECTION));
 
        if (currSun.angle_w_pad[0] <= ndotl) {
            e += currSun.SUN_ENERGY * ndotl;
        }

        else if (-currSun.angle_w_pad[0] >= ndotl) {
            e += vec3(0.0);
        }

        else {
            e += currSun.SUN_ENERGY * ((1.0 / (4.0 * currSun.angle_w_pad[0])) * ndotl * ndotl + 
                                       (0.5 * ndotl) + 
                                       (currSun.angle_w_pad[0] / 4.0));
        }
    }  

    return e; 
}

vec3 get_sphere_contribution_lamb(vec3 norm) {
    vec3 e = vec3(0.0);
    for (int i = 0; i < sphereNum; ++i) {
        SphereLight currSphere = SPHERE_LIGHTS[i];
        vec3 d_vec = currSphere.SPHERE_POSITION_RADIUS.xyz - position;
        float dist = length(d_vec);
        float half_sin = currSphere.SPHERE_POSITION_RADIUS.w / dist;

        float ndotl = dot(norm, normalize(d_vec));

        float lim_atten = max(0.0, 1.0 - pow((dist / currSphere.SPHERE_POWER_LIMIT.a), 4));
        float lim_falloff = lim_atten * lim_atten / (dist * dist + 1.0);
        float dist_attenuation = 1.0 / (4.0 * PI * pow(max(dist, currSphere.SPHERE_POSITION_RADIUS.w), 2));

        if (half_sin >= 0.999) {
            e += vec3(currSphere.SPHERE_POWER_LIMIT.rgb) * 
                 lim_atten *
                 dist_attenuation;
        }

        else if (half_sin <= ndotl) {
            e += vec3(currSphere.SPHERE_POWER_LIMIT.rgb) *
                 ndotl *
                 lim_atten *
                 dist_attenuation;
        }

        else if (-half_sin >= ndotl) {
            e += vec3(0.0);
        }

        else {
            e += currSphere.SPHERE_POWER_LIMIT.rgb * 
                 lim_atten *
                 dist_attenuation *
                ((1.0 / (4.0 * half_sin)) * ndotl * ndotl + 
                (0.5 * ndotl) + 
                (half_sin / 4.0));

        }
    }  

    return e; 
}


vec3 get_spot_contribution_lamb(vec3 norm) {
    vec3 e = vec3(0.0);
    for (int i = 0; i < spotNum; ++i) {
        SpotLight currSpot = SPOT_LIGHTS[i];
        vec3 p_vec = (currSpot.SPOT_POSITION_RADIUS.xyz - position);
        float dist = length(p_vec);
        
        
        float half_sin = currSpot.SPOT_POSITION_RADIUS.w / dist;

        //float ndotl = dot(norm, normalize(p_vec));
        float ndotl = dot(norm, normalize(p_vec));

        float lim_atten = max(0.0, 1.0 - pow((dist / currSpot.SPOT_POWER_LIMIT.a), 4));
        float lim_falloff = lim_atten * lim_atten / (dist * dist + 1.0);
        
        
        float dist_attenuation = 1.0 / (4.0 * PI * pow(max(dist, currSpot.SPOT_POSITION_RADIUS.w), 2));
       //float dist_attenuation = 1.0 / (4.0 * PI * pow(max(1.0, 1.0), 2));
       
       
       
       
        float outer = currSpot.SPOT_OUTER_INNER_LIM.x;
        float inner = currSpot.SPOT_OUTER_INNER_LIM.y;

        float angle_of_point = acos(max(0.0, dot(normalize(p_vec), normalize(currSpot.SPOT_DIRECTION))));
        float cone_atten = 0.0;
        if ((angle_of_point) < inner) {
            cone_atten = 1.0;
        }
        else if ((angle_of_point > outer)) {
            cone_atten = 0.0;
        }
        else {
            cone_atten = ((outer - angle_of_point) / (outer - inner));
        }

        if (half_sin >= 0.999) {
            e += vec3(currSpot.SPOT_POWER_LIMIT.rgb) * 
                 lim_atten *
                 dist_attenuation *
                 cone_atten;
        }

        else if (half_sin <= ndotl) {
            // change this to not use arccos eventually
            e += vec3(currSpot.SPOT_POWER_LIMIT.rgb) * 
                            ndotl *
                            lim_atten *
                            dist_attenuation *
                            cone_atten;
        }

        else if (-half_sin >= ndotl) {
            e += vec3(0.0);
        }

        else {
            e += vec3(currSpot.SPOT_POWER_LIMIT.rgb) * 
                            lim_atten *
                            dist_attenuation *
                            cone_atten *
                            ((1.0 / (4.0 * half_sin)) * ndotl * ndotl + 
                            (0.5 * ndotl) + 
                            (half_sin / 4.0));

        }
    }  

    return e; 
}


float get_dilution_aoe(float dA_var, vec3 normal) {
    // TODO: figure out if position or direction later 
    // vec3 light_dir;
    // float ndotl = dot(normal, light_dir);
    //vec3 dilute_area_total = vec3(0.0);
    float dilute_area_total = (0.0);

    for (int i = 0; i < sunNum; ++i) {
        Sun currSun = SUN_LIGHTS[i];
        // vec3 ndotl_sun = currSun.SUN_ENERGY * 
        //                  max(0.0, dot(normal, normalize(currSun.SUN_DIRECTION)));

        float ndotl_sun = max(0.0, dot(normal, normalize(currSun.SUN_DIRECTION)));

        //dilute_area_total += max(vec3(0.0), (ndotl_sun + (dA_var - 1)) / dA_var);

        dilute_area_total += ((ndotl_sun + (dA_var - 1)) / dA_var);
    }



    for (int i = 0; i < sphereNum; ++i) {
        SphereLight currSphere = SPHERE_LIGHTS[i];
        vec3 d_vec = currSphere.SPHERE_POSITION_RADIUS.xyz - position;
        // vec3 ndotl_sphere = currSphere.SPHERE_POWER_LIMIT.rgb * 
        //                     max(0.0, dot(normal, normalize(d_vec)));

        float ndotl_sphere = max(0.0, dot(normal, normalize(d_vec)));


        //dilute_area_total += max(vec3(0.0), (ndotl_sphere + (dA_var - 1)) / dA_var);
        dilute_area_total += ((ndotl_sphere + (dA_var - 1)) / dA_var);
    }

    for (int i = 0; i < spotNum; ++i) {
        SpotLight currSpot = SPOT_LIGHTS[i];
        // vec3 ndotl_spot = currSpot.SPOT_POWER_LIMIT.rgb * 
        //                   max(0.0, dot(normal, currSpot.SPOT_DIRECTION));

        float ndotl_spot = max(0.0, dot(normal, currSpot.SPOT_DIRECTION));

        //dilute_area_total += max(vec3(0.0), (ndotl_spot + (dA_var - 1)) / dA_var);

        dilute_area_total += ((ndotl_spot + (dA_var - 1)) / dA_var);
    }

    return clamp(dilute_area_total, 0.0, 1.0);
}


//* Function directly from https://thebookofshaders.com/11/
vec2 simplex(vec2 st){
   vec2 fst = floor(st);
   vec2 a = random22(fst);
   vec2 b = random22(fst + vec2(1.0, 0.0));
   vec2 c = random22(fst + vec2(0.0, 1.0));
   vec2 d = random22(fst + vec2(1.0, 1.0));
   vec2 fr = fract(st);
   vec2 u = fr*fr*(3.0-2.0*fr);
   return mix(a, b, u.x) + (c-a)*u.y *(1.0-u.x) + (d-b)*u.x*u.y;
}

//* Function directly from https://thebookofshaders.com/11/
vec3 simplex(vec3 st){
    vec3 fst = floor(st);

    vec3 a = random33(fst);
    vec3 b = random33(fst + vec3(1.0, 0.0, 0.0));
    vec3 c = random33(fst + vec3(0.0, 1.0, 0.0));
    vec3 d = random33(fst + vec3(1.0, 1.0, 0.0));

    vec3 e = random33(fst + vec3(0.0, 0.0, 1.0));
    vec3 f = random33(fst + vec3(1.0, 0.0, 1.0));
    vec3 g = random33(fst + vec3(0.0, 1.0, 1.0));
    vec3 h = random33(fst + vec3(1.0, 1.0, 1.0));

    vec3 fr = fract(st);
    vec3 u = fr * fr * (3.0 - 2.0 * fr);

    return 
        mix(
            mix(mix(a, b, u.x), mix(c, d, u.x), u.y),
            mix(mix(e, f, u.x), mix(g, h, u.x), u.y),
            u.z
        );
}

//* Function directly from https://thebookofshaders.com/13/
float fbm(vec2 st){
   float v = 0.0;
   float a = 2.5;
   vec2 shift = vec2(100.0);
   mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.5));
   for (int i = 0; i < 3; i++){
       v += a * simplex(st).x;
       st = rot * st * 2.0 + shift;
       a *= 0.6;
   }
   return v;
}

float fbm(vec3 st){
    float v = 0.0;
    float a = 2.5;
    vec3 shift = vec3(100.0);

    float angle = 0.5;
    mat3 rot = mat3(
        cos(angle),sin(angle),0.0,
        -sin(angle),cos(angle),0.0,
        0.0,0.0,1.0
    );

    for (int i = 0; i < 3; i++){
        v += a * simplex(st).x;
        st = rot * st * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}


//* background, background opacity, forground, forground opacity
//* yes, background is A and forground is B
vec4 alphaOver(vec4 A, vec4 B){
    vec3 C = B.xyz + (1.0 - B.w) * A.xyz;
    float alphaC = B.w + (1.0 - B.w) * A.w;
    return vec4(C, alphaC);
}

// To help with normal mapping: https://vulkanppp.wordpress.com/2017/07/06/week-6-normal-mapping-specular-mapping-pipeline-refactoring/


void main() {
    vec3 n = normal;
    vec3 new_normal = texture(NORMAL_SAMPLER, texCoord).rgb;
    new_normal = new_normal * 2.0 - 1.0;
    vec3 out_normal = normalize(TBN_basis * new_normal);
    
    vec3 albedo;
    vec3 lit_albedo;
    vec3 scaled_albedo;
    vec3 tonemapped_albedo;

    //* cangiante weight
    float c = 0.9;
    //* dilution weight
    float d = 0.4;
    //* cangiante color
    vec3 C = vec3(40/255.0, 30/255.0, 255/255.0);
    float dilution_area_var = 1.0;

    // paper color
    vec3 Cp = vec3(246.0/255.0, 238.0/255.0, 227.0/255.0);

    float scale = 0.1;
    vec3 noiseInput = position*scale;
    float ctrl = clamp(fbm(noiseInput)/5,0.0, 1.0);


    if (tex_type == 0) {
        vec3 c_sun = get_sun_contribution_lamb(out_normal);
        vec3 c_sphere = get_sphere_contribution_lamb(out_normal);
        vec3 c_spot = get_spot_contribution_lamb(out_normal);

        albedo = texture(TEXTURE, texCoord).rgb;
        //texture(LAMB_SAMPLER, out_normal).rgb
        lit_albedo = albedo * (c_sun + c_sphere + c_spot);

        //* 1. dilution area
        float Da = get_dilution_aoe(dilution_area_var, out_normal);

        //* 2. cangiante
        // my version:
        // treat the cangiante color as if it is a color wash that occurs under
        // existing paint (like how watercolor artists often start with a layer 
        // of light blue paint to do the shadows)

        // background
        vec4 background = vec4(C + Da * c, c);
        // foreground
        vec4 foreground = vec4(albedo, max(0.8, Da));
        vec4 both = alphaOver(background,foreground);

        vec3 Cc = both.xyz;
        float alphaC = both.w;
        
        Cc = mix(albedo, Cc, alphaC);

        // their version:
        bool accurateToPaper = false;
        if (accurateToPaper){
            Cc = (albedo + Da * c);
        }

        //* 3. dilution
        vec3 Cd = d * Da * (Cp - Cc) + Cc;

        //* 4. turbulence
        vec3 Ct;
        if (ctrl < 0.5) {
            float expv = max(1.0, 3.0 - ctrl * 4.0);
            Ct = pow(Cd, vec3(expv));
        } else {
            Ct = (ctrl - 0.5) * 2.0 * (Cp - Cd) + Cd;
        }

        bool doExposureTone = false;
        if (doExposureTone){
            vec3 exposed = exposure_scale(Ct, exposure);
            vec3 tonemapped = tonemapper(exposed, tonemap_type);
            Ct = min(Ct, Cp);
        }

        outColor = vec4(Ct, 1.0);

        float scaleLate = 2.0;
        vec3 noiseInputLate = position*scaleLate;
        float ctrlLate = clamp(fbm(noiseInputLate)/5,0.0, 1.0);
        controlColor = vec4(ctrlLate);
    }

    else if (tex_type == 1) {

        albedo = texture(CUBE_TEXTURE, out_normal).rgb;
        scaled_albedo = exposure_scale(albedo, exposure);
        tonemapped_albedo = tonemapper(scaled_albedo, tonemap_type);

        //outColor = vec4(texture(NORMAL_SAMPLER, texCoord).rgb, 1.0);
        //outColor = vec4(new_tangent, 1.0);
        //outColor = vec4(1.0, 0.0, 0.0, 1.0);
        //outColor = vec4(1.0,0.0,0.0,1.0);
        outColor = vec4(tonemapped_albedo, 1.0);
        //outColor = vec4(normalize(bitangent) * 0.5 + 0.5, 1.0);
    }

    else if (tex_type == 2) {
        //albedo = texture(CUBE_TEXTURE, reflect(EYE, n)).rgb;

        vec3 eyeDir = normalize(position - EYE);
        vec3 reflected = reflect(eyeDir, out_normal);

        albedo = texture(CUBE_TEXTURE, reflected).rgb;
        scaled_albedo = exposure_scale(albedo, exposure);
        tonemapped_albedo = tonemapper(scaled_albedo, tonemap_type);
        //outColor = vec4(1.0, 0.0, 0.0, 1.0);
        outColor = vec4(tonemapped_albedo, 1.0);
        //outColor = vec4(normalize(bitangent) * 0.5 + 0.5, 1.0);
        //outColor = vec4(new_tangent, 1.0);
        //outColor = vec4(texture(NORMAL_SAMPLER, texCoord).rgb, 1.0);
        //outColor = vec4(out_normal, 1.0);

    }

    else if (tex_type == 3) {

    }
}