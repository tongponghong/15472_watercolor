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

//https://64.github.io/tonemapping/

vec3 exposure_scale(vec3 rgb, float exposure) {
    return pow(2.0, exposure) * rgb;
}

float get_luminance(vec3 rgb) {
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
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
        //e = vec3(dist_attenuation, 0.0, 0.0);
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

// To help with normal mapping: https://vulkanppp.wordpress.com/2017/07/06/week-6-normal-mapping-specular-mapping-pipeline-refactoring/


void main() {
    vec3 n = normal;
    vec3 new_normal = texture(NORMAL_SAMPLER, texCoord).rgb;
    new_normal = new_normal * 2.0 - 1.0;
    vec3 out_normal = TBN_basis * new_normal;
    
    vec3 albedo;
    vec3 lit_albedo;
    vec3 scaled_albedo;
    vec3 tonemapped_albedo;

    //vec3 e = vec3(0.0);

    if (tex_type == 0) {
        // check why is sun energy is weird tmrw at oh 
        vec3 c_sun = get_sun_contribution_lamb(out_normal);
        vec3 c_sphere = get_sphere_contribution_lamb(out_normal);
        vec3 c_spot = get_spot_contribution_lamb(out_normal);

        albedo = texture(TEXTURE, texCoord).rgb / PI;
        //texture(LAMB_SAMPLER, out_normal).rgb
        lit_albedo = albedo * (c_sun + c_sphere + c_spot);
       
        scaled_albedo = exposure_scale(lit_albedo, exposure);
        tonemapped_albedo = tonemapper(scaled_albedo, tonemap_type);

        
        //outColor = vec4(1.0, 0.0, 0.0, 1.0);
       
        

        //outColor = vec4(texture(TEXTURE, texCoord).rgb / PI * e, 1.0);
        outColor = vec4(tonemapped_albedo, 1.0);
        //outColor = vec4(texture(LAMB_SAMPLER, out_normal).rgb, 1.0);
        //SphereLight currSphere = SPHERE_LIGHTS[0];
        //outColor = vec4(vec3(dot(out_normal, currSun.SUN_DIRECTION), 0.0, 0.0), 1.0);
        //outColor = vec4(vec3(dot(out_normal, currSun.SUN_DIRECTION)) * currSun.SUN_ENERGY, 1.0);
        //outColor = vec4(currSphere.SPHERE_POWER_LIMIT.xyz, 1.0);
        //outColor = vec4(vec3(currSun.sin_h_angle, 0.0, 0.0), 1.0);
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