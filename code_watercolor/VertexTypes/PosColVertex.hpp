#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

struct PosColVertex {
    struct { float x, y, z; } Position;
    struct { uint8_t r, g, b ,a; } Color;

    static const VkPipelineVertexInputStateCreateInfo array_input_state;
};

// make sure the struct has no padding
static_assert(sizeof(PosColVertex) == 3 * 4 + 4 * 1, "PosColVertex is packed.");
