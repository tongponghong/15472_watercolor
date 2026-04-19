#include "../Tutorial.hpp"

#include "../Helpers.hpp"
#include "../VK.hpp"

static uint32_t comp_code[] = 
#include "../spv/shaders/gaussian.comp.inl"
;

void Tutorial::ComputePipeline::create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass) 
{

    { // the set4_TEXTURE layout has a single descriptor for a sampler2D used in fragment shader:
        std::array< VkDescriptorSetLayoutBinding, 2 > bindings {
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            }
        };

        VkDescriptorSetLayoutCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_image));
        // VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set4_TEXTURE_CUBE));
        // VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set4_TEXTURE_LAMB));
    }

    { // create pipeline layout : 
        VkPushConstantRange range {
            // specifically accessible by fragment shader 
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(Push),
        };

        VkPipelineLayoutCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &set0_image,
            .pushConstantRangeCount = 1,
            // what shaders will use what portions of the push_constant struct (above)
            .pPushConstantRanges = &range,
        };

        VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout) );
    }

    {
        //* Source: https://vulkan-tutorial.com/Compute_Shader#page_The-compute-shader-stage
        VkShaderModule comp_module = rtg.helpers.create_shader_module(comp_code);
        VkPipelineShaderStageCreateInfo compute_state{};
        compute_state.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_state.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_state.module = comp_module;
        compute_state.pName = "main";

        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.layout = layout;
        pipeline_info.stage = compute_state;

        VK ( vkCreateComputePipelines(rtg.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &handle));
        
        // deallocate modules since pipeline created
        vkDestroyShaderModule(rtg.device, comp_module, nullptr);
    }
}



void Tutorial::ComputePipeline::destroy(RTG &rtg)
{
    if (set0_image != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set0_image, nullptr);
        set0_image = VK_NULL_HANDLE;
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