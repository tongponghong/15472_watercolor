#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

struct PosNorVertex {
    struct { float x, y, z; } Position;
    struct { float x, y, z; } Normal;

    static const VkPipelineVertexInputStateCreateInfo array_input_state;
};

// make sure the struct has no padding
static_assert(sizeof(PosNorVertex) == 3 * 4 + 3 * 4, "PosNorVertex is packed.");
