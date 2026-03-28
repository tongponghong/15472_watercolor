#include "Tutorial.hpp"

#include "Helpers.hpp"
#include "refsol.hpp"
#include "VK.hpp"

static uint32_t vert_code[] = 
#include "spv/shaders/background.vert.inl"
;

static uint32_t frag_code[] = 
#include "spv/shaders/background.frag.inl"
;


void Tutorial::BackgroundPipeline::create(RTG &rtg, 
                                          VkRenderPass render_pass,
                                          uint32_t subpass) 
{
    VkShaderModule vert_module = rtg.helpers.create_shader_module(vert_code);
    VkShaderModule frag_module = rtg.helpers.create_shader_module(frag_code);

    { // create pipeline layout : 
        VkPushConstantRange range {
            // specifically accessible by fragment shader 
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(Push),
        };

        VkPipelineLayoutCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr, 
            .pushConstantRangeCount = 1,
            // what shaders will use what portions of the push_constant struct (above)
            .pPushConstantRanges = &range,
        };


        VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout) );

    }

    { // create the actual pipeline
        // shader code for vertex and frag pipline stages
        std::array< VkPipelineShaderStageCreateInfo, 2 > stages 
        {
            VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert_module,
                .pName = "main"
            },
            VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag_module,
                .pName = "main"
            },
        };

        // viewport and scissor state set here (instead of being fixed)
        std::vector< VkDynamicState > dynamic_states {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = uint32_t(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data()
        };

        // no per vertex inputs 
        VkPipelineVertexInputStateCreateInfo vertex_input_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr,
        };

        // we want triangles 
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

        // pipeline only renders one viewport and one scissor rect
        VkPipelineViewportStateCreateInfo viewport_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        // rasterizer culls back faces (CCW front) and fill polygons
        VkPipelineRasterizationStateCreateInfo rasterization_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f,
        };

        // no multisampling
        VkPipelineMultisampleStateCreateInfo multisample_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
        };

        // no depth or stencil test
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_FALSE,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
        };

        // one color attachment w/ blending disabled
        std::array< VkPipelineColorBlendAttachmentState, 1 > attachment_states {
            VkPipelineColorBlendAttachmentState {
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT |
                                VK_COLOR_COMPONENT_A_BIT,
            },
        };
        VkPipelineColorBlendStateCreateInfo color_blend_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .attachmentCount = uint32_t(attachment_states.size()),
            .pAttachments = attachment_states.data(),
            .blendConstants{0.0f, 0.0f, 0.0f, 0.0f},
        };

        // one giant create_info to bundle it all together
        VkGraphicsPipelineCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = uint32_t(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &vertex_input_state,
            .pInputAssemblyState = &input_assembly_state,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterization_state,
            .pMultisampleState = &multisample_state,
            .pDepthStencilState = &depth_stencil_state,
            .pColorBlendState = &color_blend_state,
            .pDynamicState = &dynamic_state,
            .layout = layout,
            .renderPass = render_pass, 
            .subpass = subpass,
        };

        VK ( vkCreateGraphicsPipelines(rtg.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &handle));
        
        // deallocate modules since pipeline created
        vkDestroyShaderModule(rtg.device, frag_module, nullptr);
        vkDestroyShaderModule(rtg.device, vert_module, nullptr);

    }
}                                

void Tutorial::BackgroundPipeline::destroy(RTG &rtg)
{
    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if (handle != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtg.device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }

}