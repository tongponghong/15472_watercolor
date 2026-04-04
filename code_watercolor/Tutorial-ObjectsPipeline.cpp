#include "Tutorial.hpp"

#include "Helpers.hpp"
#include "refsol.hpp"
#include "VK.hpp"

static uint32_t vert_code[] = 
#include "spv/shaders/objects.vert.inl"
;

static uint32_t frag_code[] = 
#include "spv/shaders/objects.frag.inl"
;


void Tutorial::ObjectsPipeline::create(RTG &rtg, 
                                          VkRenderPass render_pass,
                                          uint32_t subpass) 
{
    VkShaderModule vert_module = rtg.helpers.create_shader_module(vert_code);
    VkShaderModule frag_module = rtg.helpers.create_shader_module(frag_code);

    { // the set0_Sun layout holds sun info to be used in the fragment shader:
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings {
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
        };
        
        VkDescriptorSetLayoutCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK ( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_SUN) );
    }

    { // the set2_Sphere layout holds spherelight info to be used in the fragment shader:
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings {
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
        };
        
        VkDescriptorSetLayoutCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK ( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set2_SPHERE) );
    }

    { // the set3_SPOT layout holds spotlight info to be used in the fragment shader:
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings {
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
        };
        
        VkDescriptorSetLayoutCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK ( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set3_SPOT) );
    }

    { // the set1_Transforms layout holds an array of Transform strucutres in a storage buffer used in the vertex shader:
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings {
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
        };
        
        VkDescriptorSetLayoutCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK ( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set1_Transforms) );
    }

    { // the set4_TEXTURE layout has a single descriptor for a sampler2D used in fragment shader:
        std::array< VkDescriptorSetLayoutBinding, 4 > bindings {
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },

            VkDescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },

            VkDescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },

            VkDescriptorSetLayoutBinding{
                .binding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
        };

        VkDescriptorSetLayoutCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set4_TEXTURE));
        // VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set4_TEXTURE_CUBE));
        // VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set4_TEXTURE_LAMB));
    }

    { // create pipeline layout : 

        // needs to be defined since then it matches the descriptor set layout to lines pipeline
        std::array< VkDescriptorSetLayout, 5 > layouts{
            set0_SUN, // (should be vk_null_handle but not possible w/o extension)
            set1_Transforms,
            set2_SPHERE,
            set3_SPOT,
            set4_TEXTURE,
        };

        VkPushConstantRange range {
            // specifically accessible by fragment shader 
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(Push),
        };

        VkPipelineLayoutCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = uint32_t(layouts.size()),
            .pSetLayouts = layouts.data(), 
            .pushConstantRangeCount = 1,
            // what shaders will use what portions of the push_constant struct (above)
            .pPushConstantRanges = &range,
        };


        VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout) );

    }

    { // create the actual pipeline
        // https://web.engr.oregonstate.edu/~mjb/vulkan/Handouts/SpecializationConstants.6pp.pdf
        spec_consts specials {
            .num_sun = rtg.scene.sunNum,
            .num_sphere = rtg.scene.sphereNum,
            .num_spot = rtg.scene.spotNum,
            .tonemap_type = rtg.tonemap_type,
        };
        
        VkSpecializationMapEntry spec_frag_entry[4] {
            {
                .constantID = 0,
                .offset = offsetof(spec_consts, num_sun),
                .size = sizeof(specials.num_sun),
            },

            {
                .constantID = 1,
                .offset = offsetof(spec_consts, num_sphere),
                .size = sizeof(specials.num_sphere),
            },

            {
                .constantID = 2,
                .offset = offsetof(spec_consts, num_spot),
                .size = sizeof(specials.num_spot),
            },

            {
                .constantID = 3,
                .offset = offsetof(spec_consts, tonemap_type),
                .size = sizeof(specials.tonemap_type),
            }
        };

        std::cout << "number of spheres: " << specials.num_sphere << std::endl;
        std::cout << "number of suns: " << specials.num_sun << std::endl;
        std::cout << "number of spots: " << specials.num_spot << std::endl;


        VkSpecializationInfo spec_frag_info {
            .mapEntryCount = 4,
            .pMapEntries = &spec_frag_entry[0],
            .dataSize = sizeof(spec_consts),
            .pData = &specials,
        };
    

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
                .pName = "main",
                .pSpecializationInfo = &spec_frag_info,
            }
        };

        // viewport and scissor state set here (instead of being fixed)
        std::vector< VkDynamicState > dynamic_states {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = uint32_t(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data()
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
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
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
            .pVertexInputState = &Vertex::array_input_state,
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

void Tutorial::ObjectsPipeline::destroy(RTG &rtg)
{
    if (set0_SUN != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set0_SUN, nullptr);
        set0_SUN = VK_NULL_HANDLE;
    }

    if (set1_Transforms != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
        set1_Transforms = VK_NULL_HANDLE;
    }

    if (set2_SPHERE != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set2_SPHERE, nullptr);
        set2_SPHERE = VK_NULL_HANDLE;
    }

    if (set3_SPOT != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set3_SPOT, nullptr);
        set3_SPOT = VK_NULL_HANDLE;
    }

    if (set4_TEXTURE != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set4_TEXTURE, nullptr);
        set4_TEXTURE = VK_NULL_HANDLE;
    }

    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if (handle != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtg.device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }
}