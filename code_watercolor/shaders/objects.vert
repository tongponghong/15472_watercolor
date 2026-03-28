#version 450

layout (location=0) in vec3 Position;
layout (location=1) in vec3 Normal;
layout (location=2) in vec4 Tangent;
layout (location=3) in vec2 TexCoord;

layout (location=0) out vec3 position;
layout (location=1) out vec3 normal;
layout (location=2) out vec3 new_tangent;
layout (location=3) out vec3 bitangent;
layout (location=4) out vec2 texCoord;
layout (location=5) out mat3 TBN_basis;

struct Transform {
    mat4 CLIP_FROM_LOCAL;
    mat4 WORLD_FROM_LOCAL;
    mat4 WORLD_FROM_LOCAL_NORMAL;
};

layout (set = 1, binding = 0, std140) readonly buffer Transforms {
    Transform TRANSFORMS[];
};

void main() {
    // col ordered so op done in A * v 
    gl_Position = TRANSFORMS[gl_InstanceIndex].CLIP_FROM_LOCAL * vec4(Position, 1.0);
    position = mat4x3(TRANSFORMS[gl_InstanceIndex].WORLD_FROM_LOCAL) * vec4(Position, 1.0);
    
    // grab from normal map 
    normal = normalize(mat3(TRANSFORMS[gl_InstanceIndex].WORLD_FROM_LOCAL_NORMAL) * Normal);
   
    new_tangent = normalize(mat3(TRANSFORMS[gl_InstanceIndex].WORLD_FROM_LOCAL_NORMAL) * Tangent.xyz);
    // bitangent sign in last comp of tangent 
    bitangent = cross(normal, new_tangent) * Tangent.w;

    TBN_basis = mat3(new_tangent, bitangent, normal);

    texCoord = vec2(TexCoord[0], 1 - TexCoord[1]);
    // need to get a tangent and bitangent 
}

