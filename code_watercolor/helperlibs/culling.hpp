#include "mathlibs/mat4.hpp"
#include <array>
#include <vector>

/**
 * largely based on https://bruop.github.io/improved_frustum_culling/
 */

struct Oriented_BB {
    std::array< vec3, 3 > basis;
    vec3 center;
    vec3 half_extents;
};

// mesh_min and mesh_max are expected to be local for this function!
Oriented_BB transform_AABB_to_OBB(mat4 view_from_local, vec3 mesh_min, vec3 mesh_max) {
    std::array< vec3, 4 > box_corners = {vec3{mesh_min.x, mesh_min.y, mesh_min.z}, 
                                         vec3{mesh_max.x, mesh_min.y, mesh_min.z},
                                         vec3{mesh_min.x, mesh_max.y, mesh_min.z},
                                         vec3{mesh_min.x, mesh_min.y, mesh_max.z}};

    std::array< vec3, 4 > xform_corners = {vec3(), vec3(), vec3(), vec3()};

    for (size_t i = 0; i < box_corners.size(); ++i) {
        vec4 newCorner = vec4{box_corners[i], 1.0f};

        xform_corners[i] = (view_from_local * newCorner).xyz();
    }

    std::array< vec3, 3 > OBB_axes = {xform_corners[1] - xform_corners[0], 
                                      xform_corners[2] - xform_corners[0], 
                                      xform_corners[3] - xform_corners[0]};
    
    vec3 OBB_extents = vec3{norm(OBB_axes[0]), norm(OBB_axes[1]), norm(OBB_axes[2])};

    vec3 OBB_center = xform_corners[0] + 0.5f * (OBB_axes[0] + OBB_axes[1] + OBB_axes[2]);

    Oriented_BB newOBB{
        .basis = {OBB_axes[0] / OBB_extents[0],
                  OBB_axes[1] / OBB_extents[1],
                  OBB_axes[2] / OBB_extents[2]},

        .center = OBB_center,

        .half_extents = OBB_extents * 0.5f,
    };

    return newOBB;
}

bool culling_test(Oriented_BB obb, float near_plane, 
                                   float far_plane, 
                                   float x_near, 
                                   float y_near) {
    { // behind the frustum 
        //vec3 sep_axis = vec3{0.0f, 0.0f, 1.0f};
        float MdotCenter = obb.center.z;

        float radius = 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            radius += fabsf(obb.basis[i].z) * obb.half_extents[i];
        }

        float min_obb = MdotCenter - radius;
        float max_obb = MdotCenter + radius;

        if (min_obb > near_plane || max_obb < far_plane) {
            return false;
        }
    }

    { // frustum normals
        std::array< vec3, 4 > frustum_norms = {vec3{ near_plane,  0.0f, -x_near}, 
                                               vec3{-near_plane,  0.0f, -x_near}, 
                                               vec3{0.0f,  -near_plane, -y_near}, 
                                               vec3{0.0f,   near_plane, -y_near},
                                               };
        
        for (vec3 normal : frustum_norms) {
            float MdotX = fabsf(normal.x);
            float MdotY = fabsf(normal.y);
            float MdotZ = (normal.z);
            float MdotCenter = dot(normal, obb.center);

            float radius = 0.0f;
            for (size_t i = 0; i < 3; ++i) {
                radius += fabsf(dot(normal, obb.basis[i])) * obb.half_extents[i];
            }

            float min_obb = MdotCenter - radius;
            float max_obb = MdotCenter + radius;

            float tau = x_near * MdotX + y_near * MdotY;

            float left_frustum_extent = near_plane * MdotZ - tau;
            float right_frustum_extent = near_plane * MdotZ + tau;

            if (left_frustum_extent < 0.0f) {
                left_frustum_extent *= far_plane / near_plane;
            }

            if (right_frustum_extent > 0.0f) {
                right_frustum_extent *= far_plane / near_plane;
            }

            if (min_obb > right_frustum_extent || max_obb < left_frustum_extent) {
                return false;
            }
        }    
    }

    return true;
}



