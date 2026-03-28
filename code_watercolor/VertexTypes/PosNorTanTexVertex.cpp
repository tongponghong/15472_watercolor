#include "PosNorTanTexVertex.hpp"

#include <array>

static std::array< VkVertexInputBindingDescription, 1 > bindings {
    VkVertexInputBindingDescription {
        .binding = 0,
        .stride = sizeof(PosNorTanTexVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }
};

static std::array< VkVertexInputAttributeDescription, 4 > attributes {
    // stores the position as XYZ (typed as RGB)
    VkVertexInputAttributeDescription {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(PosNorTanTexVertex, Position),
    },

    // actually the normal
    VkVertexInputAttributeDescription {
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(PosNorTanTexVertex, Normal),
    },

    // the tangent 
    VkVertexInputAttributeDescription {
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(PosNorTanTexVertex, Tangent),
    },

    // the texcoord
    VkVertexInputAttributeDescription {
        .location = 3,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(PosNorTanTexVertex, TexCoord),
    },
};

const VkPipelineVertexInputStateCreateInfo PosNorTanTexVertex::array_input_state {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = uint32_t(bindings.size()),
    .pVertexBindingDescriptions = bindings.data(),
    .vertexAttributeDescriptionCount = uint32_t(attributes.size()),
    .pVertexAttributeDescriptions = attributes.data(),
};

