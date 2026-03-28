#include "Tutorial.hpp"
#include "mathlibs/mat4.hpp"
#include "VK.hpp"
#include "helperlibs/S72_loader/S72.hpp"
#include "helperlibs/timer.hpp"
#include "helperlibs/rgb_decoders.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "helperlibs/stb_image_lib/stb_image.h"

#include <GLFW/glfw3.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <glm/glm/glm.hpp>

Tutorial::Tutorial(RTG &rtg_) : rtg(rtg_) {
	// select a depth format, at least one of these two formats must be supported according to spec, but neither required
	
	depth_format = rtg.helpers.find_image_format(
		{VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);

	{ // create render pass
		// attachments:
		std::array< VkAttachmentDescription, 2 > attachments{
			VkAttachmentDescription{ // 0 - color attachment (format determined by output surface)ßß
				.format = rtg.surface_format.format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				// how to load data before rendering
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				// how to write data back after rendering
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				// what layout th imag will be transitioned to before load
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				// what layout the image will be transitioned to after store 
				.finalLayout = rtg.present_layout,
			},
			VkAttachmentDescription{
				.format = depth_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		};

		// subpass(es):
		VkAttachmentReference color_attachment_ref{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkAttachmentReference depth_attachment_ref{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_ref,
			.pDepthStencilAttachment = &depth_attachment_ref,
		};

		// this defers the image load actions for the attachments:
		std::array< VkSubpassDependency, 2 > dependencies {
			// finish all work in color attachment output stage
			// then do layout transition
			// then start work in the color attachment output stage again
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			},
			// 
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			}
		};

		VkRenderPassCreateInfo create_info {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = uint32_t(dependencies.size()),
			.pDependencies = dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &create_info, nullptr, &render_pass) );
	}

	{ // create shadow render pass
		// attachments:
		std::array< VkAttachmentDescription, 1 > attachments{
			// only should be a depth attachment
			// maybe change loadop eventually for optimization purposes
			VkAttachmentDescription{
				.format = depth_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			},
		};

		// subpass(es):
		VkAttachmentReference depth_attachment_ref{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.pDepthStencilAttachment = &depth_attachment_ref,
		};

		// https://github.com/zhang-jian/bookshelf/blob/master/Computer%20Science/Vulkan%20Cookbook.pdf

		// this defers the image load actions for the attachments:
		std::array< VkSubpassDependency, 2 > dependencies {
			// finish all work in color attachment output stage
			// then do layout transition
			// then start work in the color attachment output stage again
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
			},
			// 
			VkSubpassDependency{
				.srcSubpass = 0,
				.dstSubpass = VK_SUBPASS_EXTERNAL,
				.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			}
		};

		VkRenderPassCreateInfo create_info {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = uint32_t(dependencies.size()),
			.pDependencies = dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &create_info, nullptr, &shadow_render_pass) );
	}

	{ // create command pool
		VkCommandPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = rtg.graphics_queue_family.value(),
		};

		VK( vkCreateCommandPool(rtg.device, &create_info, nullptr, &command_pool));
	}

	// double check about this 
	// if (shadow_atlas_image.handle != VK_NULL_HANDLE) {
	// 	destroy_framebuffers();
	// }

	// create shadow atlas image 
	shadow_atlas_image = rtg.helpers.create_image(
		shadowAtlasExtent,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped
	);

	{ // create an image view of the depth image
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = shadow_atlas_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = depth_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};

		VK( vkCreateImageView(rtg.device, &create_info, nullptr, &shadow_atlas_image_view) );
	}

	std::array< VkImageView, 1 > attachments{
		shadow_atlas_image_view
	};

	VkFramebufferCreateInfo create_info {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = render_pass,
		.attachmentCount = uint32_t(attachments.size()),
		.pAttachments = attachments.data(),
		.width = shadowAtlasExtent.width,
		.height = shadowAtlasExtent.height,
		.layers = 1,
	};

	VK( vkCreateFramebuffer(rtg.device, &create_info, nullptr, &shadowmap_framebuffer) );


	shadows_pipeline.create(rtg, shadow_render_pass, 0);
	background_pipeline.create(rtg, render_pass, 0);
	lines_pipeline.create(rtg, render_pass, 0);
	objects_pipeline.create(rtg, render_pass, 0);
	
	{ // create descriptor pool
		uint32_t per_workspace = uint32_t(rtg.workspaces.size());
		std::array< VkDescriptorPoolSize, 2 > pool_sizes {
			VkDescriptorPoolSize {
				// one desciptor per set, two set per workspace
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1 * per_workspace, 
			},
			VkDescriptorPoolSize{
				// one descriptor per set, one set per workspace 
				.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 4 * per_workspace,
			}
		};

		VkDescriptorPoolCreateInfo create_info {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			//because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.flags = 0,
			.maxSets = 5 * per_workspace,  // five sets per workspace 
			.poolSizeCount = uint32_t(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data(),
		};

		// create the pool!
		VK(vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &descriptor_pool))
	}
	
	workspaces.resize(rtg.workspaces.size());
	for (Workspace &workspace : workspaces) {
		{ // allocate command buffer:
			VkCommandBufferAllocateInfo alloc_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = command_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
			};
			VK( vkAllocateCommandBuffers(rtg.device, &alloc_info, &workspace.command_buffer) );
		}

		workspace.Camera_src = rtg.helpers.create_buffer(
			sizeof(LinesPipeline::Camera),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // going to have GPU copy from this memory
			//host-visible memory, coherent (no special sync needed)
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			Helpers::Mapped // get a ptr to mem
		);

		workspace.Camera = rtg.helpers.create_buffer(
			sizeof(LinesPipeline::Camera),
			// going to use as a uniform buffer and going to have GPU copy into this mem
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // GPU local memory
			Helpers::Unmapped // don't get a pointer
		);
		
		{ // allocate descriptor set for Camera descriptor
			VkDescriptorSetAllocateInfo alloc_info {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &lines_pipeline.set0_Camera,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.Camera_descriptors) );
		}    
		
		{ // allocate descriptor set for Sunlight descriptor
			VkDescriptorSetAllocateInfo alloc_info {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &objects_pipeline.set0_SUN,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.Sunlights_descriptors) );
		}

		{ // allocate descriptor set for Spherelight descriptor
			VkDescriptorSetAllocateInfo alloc_info {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &objects_pipeline.set2_SPHERE,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.Spherelights_descriptors) );
		}

		{ // allocate descriptor set for Spotlight descriptor
			VkDescriptorSetAllocateInfo alloc_info {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &objects_pipeline.set3_SPOT,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.Spotlights_descriptors) );
		}

		{ // allocate descriptor set for Transforms descriptor
			VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &objects_pipeline.set1_Transforms,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.Transforms_descriptors));

		}

		// descriptor write
		{ // point descriptor to Camera buffer:
			VkDescriptorBufferInfo Camera_info {
				.buffer = workspace.Camera.handle,
				.offset = 0,
				.range = workspace.Camera.size,
			};

			std::array< VkWriteDescriptorSet, 1 > writes{
				VkWriteDescriptorSet {
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.Camera_descriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &Camera_info,
				},
			};

			vkUpdateDescriptorSets(
				rtg.device, // device
				uint32_t(writes.size()), // descriptorWriteCount
				writes.data(), // pDescriptorWrites
				0, // descriptorCopyCount
				nullptr //pDescriptorCopies
			);

		}
	}

	{ // create object vertices
		//std::vector< PosNorTexVertex > vertices;

		size_t totalBytes = 0;

		for (const auto &mesh : rtg.scene.meshes) { // should be 48
			assert(sizeof(PosNorTanTexVertex) == 48);
			totalBytes += mesh.second.count * sizeof(PosNorTanTexVertex);
		}

		Helpers::AllocatedBuffer staging_buffer = rtg.helpers.create_buffer(
			totalBytes, 
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			Helpers::Mapped
		);

		object_vertices = rtg.helpers.create_buffer(
			totalBytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			Helpers::Unmapped
		);

		uint32_t curr_buffer_offset = 0;

		char *stageBuffPtr = reinterpret_cast<char *>(staging_buffer.allocation.data());

		// read in vertices from mesh files:
		for (auto &meshDict : rtg.scene.meshes) {
			auto &mesh = meshDict.second;

			std::ifstream file(mesh.attributes.begin()->second.src.path, std::ios::in | std::ios::binary);
			
			if (file.is_open()) {
				size_t fileSize = mesh.count * sizeof(PosNorTanTexVertex);
				
				file.seekg (0, std::ios::beg);

				file.read(stageBuffPtr + curr_buffer_offset, fileSize);
				file.close();

				std::cout << "loaded mesh: " << mesh.attributes.begin()->second.src.path 
				          << "with offset: " << curr_buffer_offset << std::endl;
				
				PosNorTanTexVertex *startVert = (PosNorTanTexVertex *)staging_buffer.allocation.data() + curr_buffer_offset / sizeof(PosNorTanTexVertex);
				for (uint32_t i = 0; i < mesh.count; ++i) {
					// std::cout << "Position: " << (startVert + i )->Position.x << (startVert + i )->Position.y << (startVert + i )->Position.z << std::endl;
					vec3 currPos = vec3{(startVert + i)->Position.x, (startVert + i)->Position.y, (startVert + i)->Position.z};
					mesh.mesh_min = min_comps(mesh.mesh_min, currPos);
					mesh.mesh_max = max_comps(mesh.mesh_max, currPos);
				}

				std::cout << "here are the min and max of the mesh\n" << std::endl;
				print_vector_3x1(mesh.mesh_min);
				print_vector_3x1(mesh.mesh_max);

				mesh.offset_in_static_buffer = curr_buffer_offset;

				curr_buffer_offset += fileSize;
			}

			else {
				std::cout << "File wasn't read" << std::endl;
				curr_buffer_offset += 0;
			}
		} 

		
		
		// copy data to buffer:
		rtg.helpers.transfer_to_buffer(staging_buffer.allocation.data(), totalBytes, object_vertices);

		rtg.helpers.destroy_buffer(std::move(staging_buffer));
	}

	{ // make some textures
		textures_in_use.reserve(10);

		{ // texture 0 will be a dark grey/light grey checkerboard with a red square at the origin.
			// actually make the texture:
			uint32_t size = 128;
			std::vector< uint32_t > data;
			data.reserve(size * size);
			for (uint32_t y = 0; y < size; ++y) {
				float fy = (y + 0.5f) / float(size);
				for (uint32_t x = 0; x < size; ++x) {
					float fx = (x + 0.5f) / float(size);
					// highlight origin
					// if      (fx < 0.05f && fy < 0.05f) data.emplace_back(0xff0000ff); // red
					// else if ( (fx < 0.5f) == (fy < 0.5f)) data.emplace_back(0xff444444); // dark grey

					// else data.emplace_back(0xffbbbbbb); // light grey
					(void) fx;
					(void) fy;

					data.emplace_back(0xffbbbbbb);
				}
			}
			assert(data.size() == size * size);

			//obj_Material currMaterial;

			textures_in_use.emplace_back(rtg.helpers.create_image(
				VkExtent2D{ .width = size, .height = size }, // size of image
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				Helpers::Unmapped
			));

			strings_to_texture_idxs.insert({"blank", 0});

			// transfer data
			rtg.helpers.transfer_to_image(data.data(), sizeof(data[0]) * data.size(), textures_in_use.back());

		}

		{ // texture 1 is a default normal texture
			// actually make the texture
			uint32_t size = 1;
			std::vector< uint32_t > data;
			data.reserve(size * size);
			uint8_t a = 0xff;
			data.emplace_back((128) | 
							  (128 << 8) | 
							  (255 << 16) | 
							  ((uint32_t(a) << 24) ));
			assert(data.size() == size * size);
			
			// make a place for the texture to live on the GPU 
			textures_in_use.emplace_back(rtg.helpers.create_image(
				VkExtent2D{ .width = size, .height = size }, // size of image
				VK_FORMAT_R8G8B8A8_UNORM, // how to interpret image data (in this case, SRGB-encoded 8-bit RGBA)
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // should be device_local
				Helpers::Unmapped
			));

			//std::cout << "size of data[0]" << sizeof(data[0]) << std::endl;
			strings_to_texture_idxs.insert({"defaultNorm", 1});
			
			rtg.helpers.transfer_to_image(data.data(), sizeof(data[0]) * data.size(), textures_in_use.back());
		}

		{ // make a default cube
			std::vector< uint32_t > data(16 * 96);
			uint32_t rgbe_res;

			stbi_uc R = 1;
			stbi_uc G = 1;
			stbi_uc B = 1;
			stbi_uc E = 128;

			float newR = powf(2.0f, float(E) - 128.0f) * ((float(R) + 0.5f) / 256.0f);
			float newG = powf(2.0f, float(E) - 128.0f) * ((float(G) + 0.5f) / 256.0f);
			float newB = powf(2.0f, float(E) - 128.0f) * ((float(B) + 0.5f) / 256.0f);
			rgb9_e5_from_rgbe8(newR, newG, newB, &rgbe_res);

			std::fill(data.begin(), data.end(), rgbe_res);
			
			// make a place for the texture to live on the GPU 
			cube_textures.emplace_back(rtg.helpers.create_image(
				VkExtent2D{ .width = (uint) 16, .height = (uint) 16*6}, // size of image
				VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, // how to interpret image data 
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // should be device_local
				Helpers::Unmapped
			));

			std::cout << "size of data[0]" << sizeof(data[0]) << std::endl;
			std::cout << "size of data" << data.size() << std::endl;

			strings_to_cube_texture_idxs.insert({"defaultCube", 0});

			rtg.helpers.transfer_to_cube_image(data.data(), sizeof(data[0]) * data.size(), cube_textures.back());
		}

		// since textures 0 and 1 of 2D and texture 0 of cube are already filled 
		uint32_t currentIndex = 2;
		uint32_t currentCubeTexIndex = 1;

		{ // get rest of textures from images:
			for (auto &texture : rtg.scene.textures) {
				int tex_img_width, tex_img_height, channels;
				std::cout << texture.second.src << std::endl;
				std::cout << texture.first << std::endl;
				// get bytes of the image
				stbi_uc *img = stbi_load(texture.second.path.c_str(), &tex_img_width, &tex_img_height, &channels, 4);
				uint32_t image_byte_size = 4 * tex_img_width * tex_img_height;
				
				std::cout << "here are the number of channels: " << channels << std::endl;
				//std::cout << "here is the image format: " << texture.second.format << std::endl;
				
				if (texture.second.format == S72::Texture::Format::srgb){
					textures_in_use.emplace_back(rtg.helpers.create_image(
						VkExtent2D{ .width = (uint) tex_img_width, .height = (uint) tex_img_height}, // size of image
						VK_FORMAT_R8G8B8A8_SRGB, // how to interpret image data (in this case, SRGB-encoded 8-bit RGBA)
						VK_IMAGE_TILING_OPTIMAL,
						VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // should be device_local
						Helpers::Unmapped
					));
					rtg.helpers.transfer_to_image(img, image_byte_size, textures_in_use.back());
					strings_to_texture_idxs.insert({texture.second.src, currentIndex});
				
					++currentIndex;
				}
				
				else if (texture.second.format == S72::Texture::Format::linear) {
					textures_in_use.emplace_back(rtg.helpers.create_image(
						VkExtent2D{ .width = (uint) tex_img_width, .height = (uint) tex_img_height}, // size of image
						VK_FORMAT_R8G8B8A8_UNORM, // how to interpret image data (in this case, Linear-encoded 8-bit RGBA)
						VK_IMAGE_TILING_OPTIMAL,
						VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // should be device_local
						Helpers::Unmapped
					));
					
					rtg.helpers.transfer_to_image(img, image_byte_size, textures_in_use.back());
					//std::cout << "This is a linear image: " << texture.second.src << std::endl;
					strings_to_texture_idxs.insert({texture.second.src, currentIndex});
				
					++currentIndex;
				}

				// worked with Enrique 
				else if (texture.second.format == S72::Texture::Format::rgbe) { 
					// auto rgbe_to_rgb_prime = [](stbi_uc channel, stbi_uc exp) {
					// 	return float(pow(2, exp - 128)) * float(float(channel) + 0.5 / 256);
					// };
					std::cout << "Source for texture rgbe: " << texture.second.src << std::endl;
					std::vector< uint32_t > data;
					data.reserve(tex_img_width * tex_img_height);
					// https://registry.khronos.org/OpenGL/specs/gl/glspec30.pdf page 181-182
					for (uint32_t i = 0; i < image_byte_size; i += 4) {
						stbi_uc R = *(img + i);
						stbi_uc G = *(img + i + 1);
						stbi_uc B = *(img + i + 2);
						stbi_uc E = *(img + i + 3);
	
						// float R_prime, G_prime, B_prime;
						uint32_t rgbe_res;

						if (R == 0 && G == 0 && B == 0 && E == 0) {
							rgbe_res = 0;
						}

						else {
							float newR = powf(2.0f, float(E) - 128.0f) * ((float(R) + 0.5f) / 256.0f);
							float newG = powf(2.0f, float(E) - 128.0f) * ((float(G) + 0.5f) / 256.0f);
							float newB = powf(2.0f, float(E) - 128.0f) * ((float(B) + 0.5f) / 256.0f);
							rgb9_e5_from_rgbe8(newR, newG, newB, &rgbe_res);
						}

						data[i / 4] = rgbe_res;
						//std::cout << rgbe_res << std::endl;
					}
					//std::cout << "oops" << std::endl;
					cube_textures.emplace_back(rtg.helpers.create_image(
						VkExtent2D{ .width = (uint) tex_img_width, .height = (uint) tex_img_height}, // size of image
						VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, // how to interpret image data 
						VK_IMAGE_TILING_OPTIMAL,
						VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // should be device_local
						Helpers::Unmapped
					));
					rtg.helpers.transfer_to_cube_image(data.data(), image_byte_size, cube_textures.back());
					

					strings_to_cube_texture_idxs.insert({texture.second.src, currentCubeTexIndex});
				
					++currentCubeTexIndex;
				}
				stbi_image_free(img);
				
			}
			
		}

		{// load in color, roughness, and metalness textures as well:
            uint32_t size = 1;
            std::vector< uint32_t > data;
            for (auto &material : rtg.scene.materials) {
				std::cout << "These are the materials in the scene: " << material.second.name << std::endl;
                data.clear();

                if (S72::Material::Lambertian *brdfPtr = std::get_if<S72::Material::Lambertian> (&material.second.brdf)) {
                    if (S72::color *colorPtr = std::get_if<S72::color> (&brdfPtr->albedo)) {
                        S72::color currColor = *colorPtr;
						
                        
                        data.reserve(size * size);
                        uint8_t a = 0xff;
                        data.emplace_back( (uint32_t((currColor.r) * 255)) | 
                                           (uint32_t((currColor.g) * 255) << 8) | 
                                           (uint32_t((currColor.b) * 255) << 16) | 
                                           (uint32_t(a) << 24) );
                        assert(data.size() == size * size);
                    }
                }

                else if (S72::Material::PBR *brdfPtr = std::get_if<S72::Material::PBR> (&material.second.brdf)) {
                    if (S72::color *colorPtr = std::get_if<S72::color> (&brdfPtr->albedo)) {
                        S72::color currColor = *colorPtr;

                        data.reserve(size * size);
                        uint8_t a = 0xff;
                        data.emplace_back( (uint32_t((currColor.r) * 255)) | 
                                           (uint32_t((currColor.g) * 255) << 8) | 
                                           (uint32_t((currColor.b) * 255) << 16) | 
                                           (uint32_t(a) << 24) );
                        assert(data.size() == size * size);

                    }
                }

				// // change this ordering in the morning 
				// if (S72::Material::PBR *brdfPtr = std::get_if<S72::Material::PBR> (&material.second.brdf)) {
                //     if (float *roughnessPtr = std::get_if<float> (&brdfPtr->roughness)) {
                //         float currRoughness = *roughnessPtr;

				// 		data.reserve(size * size);
                //         uint8_t a = 0xff;
                //         data.emplace_back( (uint32_t((currRoughness) * 255)) | 
                //                            (uint32_t((currRoughness) * 255) << 8) | 
                //                            (uint32_t((currRoughness) * 255) << 16) | 
                //                            (uint32_t(a) << 24) );
                //         assert(data.size() == size * size);
				// 	}
				// }

				// // change this ordering in the morning 
				// if (S72::Material::PBR *brdfPtr = std::get_if<S72::Material::PBR> (&material.second.brdf)) {
                //     if (float *metalnessPtr = std::get_if<float> (&brdfPtr->metalness)) {
                //         float currMetalness = *metalnessPtr;

				// 		data.reserve(size * size);
                //         uint8_t a = 0xff;
                //         data.emplace_back( (uint32_t((currMetalness) * 255)) | 
                //                            (uint32_t((currMetalness) * 255) << 8) | 
                //                            (uint32_t((currMetalness) * 255) << 16) | 
                //                            (uint32_t(a) << 24) );
                //         assert(data.size() == size * size);


				// 	}
				// }
                
                textures_in_use.emplace_back(rtg.helpers.create_image(
                    VkExtent2D{ .width = size, .height = size }, // size of image
                    VK_FORMAT_R8G8B8A8_UNORM, // how to interpret image data (in this case, SRGB-encoded 8-bit RGBA)
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // should be device_local
                    Helpers::Unmapped
                ));

                strings_to_texture_idxs.insert({material.second.name, currentIndex});
                ++currentIndex;

				std::cout << "data size: " << data.size() << std::endl;
				if (data.size() > 0) {
                	rtg.helpers.transfer_to_image(data.data(), sizeof(data[0]) * data.size(), textures_in_use.back());
				}
			}
        }

		// build up the materials so we can use them in descriptor layouts 
		for (auto &material : rtg.scene.materials) {
			Obj_Material currObjMaterial;
			std::cout << "Current material is: " << material.second.name << std::endl;

			// lambertian material 
			if (S72::Material::Lambertian *brdfPtr = std::get_if<S72::Material::Lambertian> (&material.second.brdf)) {
				// match the correct texture (not a single color)
            	if (S72::Texture **texPtr = std::get_if<S72::Texture *> (&brdfPtr->albedo)) {
					std::cout << "This is the current texPtr source: " << (*texPtr)->src << std::endl;
					currObjMaterial.texture_idx = strings_to_texture_idxs[(*texPtr)->src];
				}
				// match the correct color texture
				else if (S72::color *colorPtr = std::get_if<S72::color> (&brdfPtr->albedo)) { 
					std::cout << "This is a color, curr texPtr source: " << std::endl;
					(void) colorPtr;
					currObjMaterial.texture_idx = strings_to_texture_idxs[(material.second.name)];
				}
			}

			// it is a PBR material, get the correct textures
			else if (S72::Material::PBR *brdfPtr = std::get_if<S72::Material::PBR> (&material.second.brdf)) {
				// match the correct texture (not a single color)
            	if (S72::Texture **texPtr = std::get_if<S72::Texture *> (&brdfPtr->albedo)) {
					std::cout << "This is the current texPtr source: " << (*texPtr)->src << std::endl;
					currObjMaterial.texture_idx = strings_to_texture_idxs[(*texPtr)->src];
				}
				// match the correct color texture
				else if (S72::color *colorPtr = std::get_if<S72::color> (&brdfPtr->albedo)) { 
					std::cout << "This is a color, curr texPtr source: " << std::endl;
					(void) colorPtr;
					currObjMaterial.texture_idx = strings_to_texture_idxs[(material.second.name)];
				}
			}


			// figure out if u need to attach a normal map
			if (material.second.normal_map) {
				std::cout << "This is the current normal map source: " << material.second.normal_map->src << std::endl;
				currObjMaterial.normal_map_tex_idx = strings_to_texture_idxs[(material.second.normal_map->src)];
			}
			else {
				currObjMaterial.normal_map_tex_idx = strings_to_texture_idxs["defaultNorm"];
				std::cout << "defaultNorm maps to: " << strings_to_texture_idxs["defaultNorm"] << std::endl;
			}

			obj_materials.emplace_back(currObjMaterial);
			strings_to_material_idxs.insert({material.second.name, obj_materials.size() - 1});
		}

	

        for (auto i : strings_to_texture_idxs) {
            std::cout << "In strings_to_tex_idxs: " << i.first << ", " << i.second << std::endl;
        }
		for (auto i : strings_to_cube_texture_idxs) {
            std::cout << "In strings_to_cube_tex_idxs before envs: " << i.first << ", " << i.second << std::endl;
        }
		for (auto i : strings_to_material_idxs) {
            std::cout << "In strings_to_materials_idxs: " << i.first << ", " << i.second << std::endl;
        }

		std::cout << "loading in the cube lambertian lookup" << std::endl;
		if (rtg.lamb_lookup_tex != "") { // handle the lambertian lookup table 
			std::string lambPath = rtg.lamb_lookup_tex; 
			int tex_img_width, tex_img_height, channels;
			std::cout << "Source for texture rgbe: " << lambPath << std::endl;
			std::vector< uint32_t > data;

			stbi_uc *img = stbi_load(lambPath.c_str(), &tex_img_width, &tex_img_height, &channels, 4);
			data.reserve(tex_img_width * tex_img_height);
			uint32_t image_byte_size = 4 * tex_img_width * tex_img_height;

			// https://registry.khronos.org/OpenGL/specs/gl/glspec30.pdf page 181-182
			for (uint32_t i = 0; i < image_byte_size; i += 4) {
				stbi_uc R = *(img + i);
				stbi_uc G = *(img + i + 1);
				stbi_uc B = *(img + i + 2);
				stbi_uc E = *(img + i + 3);

				// float R_prime, G_prime, B_prime;
				uint32_t rgbe_res;

				if (R == 0 && G == 0 && B == 0 && E == 0) {
					rgbe_res = 0;
				}

				else {
					float newR = powf(2.0f, float(E) - 128.0f) * ((float(R) + 0.5f) / 256.0f);
					float newG = powf(2.0f, float(E) - 128.0f) * ((float(G) + 0.5f) / 256.0f);
					float newB = powf(2.0f, float(E) - 128.0f) * ((float(B) + 0.5f) / 256.0f);
					rgb9_e5_from_rgbe8(newR, newG, newB, &rgbe_res);
				}

				data[i / 4] = rgbe_res;
				//std::cout << rgbe_res << std::endl;
			}
			//std::cout << "oops" << std::endl;
			lambertian_lookup_tex.emplace_back(rtg.helpers.create_image(
				VkExtent2D{ .width = (uint) tex_img_width, .height = (uint) tex_img_height}, // size of image
				VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, // how to interpret image data 
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // should be device_local
				Helpers::Unmapped
			));
			rtg.helpers.transfer_to_cube_image(data.data(), image_byte_size, lambertian_lookup_tex.back());
			stbi_image_free(img);
		}

		else {
			std::cout << "no lamb lookup" << std::endl;
			std::vector< uint32_t > data(16 * 96);
			uint32_t rgbe_res;

			stbi_uc R = 0;
			stbi_uc G = 0;
			stbi_uc B = 0;
			stbi_uc E = 128;

			float newR = powf(2.0f, float(E) - 128.0f) * ((float(R) + 0.5f) / 256.0f);
			float newG = powf(2.0f, float(E) - 128.0f) * ((float(G) + 0.5f) / 256.0f);
			float newB = powf(2.0f, float(E) - 128.0f) * ((float(B) + 0.5f) / 256.0f);
			rgb9_e5_from_rgbe8(newR, newG, newB, &rgbe_res);

			std::fill(data.begin(), data.end(), rgbe_res);
			
			// make a place for the texture to live on the GPU 
			lambertian_lookup_tex.emplace_back(rtg.helpers.create_image(
				VkExtent2D{ .width = (uint) 16, .height = (uint) 16*6}, // size of image
				VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, // how to interpret image data 
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // should be device_local
				Helpers::Unmapped
			));

			std::cout << "size of data[0]" << sizeof(data[0]) << std::endl;
			std::cout << "size of data" << data.size() << std::endl;
			rtg.helpers.transfer_to_cube_image(data.data(), sizeof(data[0]) * data.size(), lambertian_lookup_tex.back());
		}
    }

	for (auto i : strings_to_cube_texture_idxs) {
		std::cout << "In strings_to_cube_tex_idxs: " << i.first << ", " << i.second << std::endl;
	}

	{ // make image views for the textures
		material_views.reserve(textures_in_use.size());

		for (Helpers::AllocatedImage const &image : textures_in_use) {
			VkImageViewCreateInfo create_info {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.flags = 0,
				.image = image.handle,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = image.format,
				// .components sets swizzling and is fine when zero initialized 
				.subresourceRange {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};

			VkImageView image_view = VK_NULL_HANDLE;
			VK( vkCreateImageView(rtg.device, &create_info, nullptr, &image_view) );

			material_views.emplace_back(image_view);
		}
		//assert(material_views.size() == textures_in_use.size());

		// create separate image view for cubes 
		cube_texture_views.reserve(cube_textures.size());
		for (Helpers::AllocatedImage const &image : cube_textures) {
			VkImageViewCreateInfo create_info {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.flags = 0,
				.image = image.handle,
				.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
				.format = image.format,
				// .components sets swizzling and is fine when zero initialized 
				.subresourceRange {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 6,
				},
			};

			VkImageView image_view = VK_NULL_HANDLE;
			VK( vkCreateImageView(rtg.device, &create_info, nullptr, &image_view) );

			cube_texture_views.emplace_back(image_view);
		}

		lambertian_lookup_views.reserve(lambertian_lookup_tex.size());
		for (Helpers::AllocatedImage const &image : lambertian_lookup_tex) {
			VkImageViewCreateInfo create_info {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.flags = 0,
				.image = image.handle,
				.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
				.format = image.format,
				// .components sets swizzling and is fine when zero initialized 
				.subresourceRange {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 6,
				},
			};

			VkImageView image_view = VK_NULL_HANDLE;
			VK( vkCreateImageView(rtg.device, &create_info, nullptr, &image_view) );

			lambertian_lookup_views.emplace_back(image_view);
		}
	}

	{ // make a sampler for textures
		VkSamplerCreateInfo create_info_2D_sampler{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.flags = 0,
			// change back to nearest eventually
			.magFilter = VK_FILTER_NEAREST,
			.minFilter = VK_FILTER_NEAREST,
			// change back to nearest eventually
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			// control how texture repeats or doesn't
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE,
			.maxAnisotropy = 0.0f, // doesnt matter bc anisotropy isn't enabled
			.compareEnable = VK_FALSE,
			.compareOp = VK_COMPARE_OP_ALWAYS, // doesn't matter as well bc compare not enabled
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		VK( vkCreateSampler(rtg.device, &create_info_2D_sampler, nullptr, &material_sampler) );

		// create a new sampler that uses linear sampling 
		VkSamplerCreateInfo create_info_cube_sampler{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.flags = 0,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			// control how texture repeats or doesn't
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE,
			.maxAnisotropy = 0.0f, // doesnt matter bc anisotropy isn't enabled
			.compareEnable = VK_FALSE,
			.compareOp = VK_COMPARE_OP_ALWAYS, // doesn't matter as well bc compare not enabled
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		VK( vkCreateSampler(rtg.device, &create_info_cube_sampler, nullptr, &cube_texture_sampler) );
	}

	{ // create texture descriptor pool
		// add the cube texture descriptor as well
		uint32_t per_texture = uint32_t(obj_materials.size());
		uint32_t per_cube_texture = uint32_t(cube_textures.size());
		uint32_t per_lamb_texture = uint32_t(lambertian_lookup_tex.size());

		std::array< VkDescriptorPoolSize, 1> pool_sizes{
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 4 * (per_texture + 
				                        per_cube_texture + 
								        per_lamb_texture),
			},
		};

		VkDescriptorPoolCreateInfo create_info {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, // CREATE_FREE_DESCRIPTOR_SET_BIT is NOT included -> *can't* free individual descirptors allocated from this pool
			.maxSets = 1 * per_texture + 1 * per_cube_texture + 1 * per_lamb_texture, // one set per texture
			.poolSizeCount = uint32_t(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &texture_descriptor_pool) );
	}

	{ // allocate and write the texture descriptor sets
		
		// allocate the descriptors (using the same alloc_info):
		VkDescriptorSetAllocateInfo alloc_info {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = texture_descriptor_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &objects_pipeline.set4_TEXTURE,
		};
		material_descriptors.assign(obj_materials.size(), VK_NULL_HANDLE);
		for (VkDescriptorSet &descriptor_set : material_descriptors) {
			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &descriptor_set) );
		}

		// VkDescriptorSetAllocateInfo alloc_cube_info {
		// 	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		// 	.descriptorPool = texture_descriptor_pool,
		// 	.descriptorSetCount = 1,
		// 	.pSetLayouts = &objects_pipeline.set4_TEXTURE_CUBE,
		// };
		cube_texture_descriptors.assign(cube_textures.size(), VK_NULL_HANDLE);
		for (VkDescriptorSet &descriptor_set : cube_texture_descriptors) {
			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &descriptor_set) );
		}
		
		// VkDescriptorSetAllocateInfo alloc_lamb_info {
		// 	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		// 	.descriptorPool = texture_descriptor_pool,
		// 	.descriptorSetCount = 1,
		// 	.pSetLayouts = &objects_pipeline.set4_TEXTURE_LAMB,
		// };
		lamb_texture_descriptors.assign(lambertian_lookup_tex.size(), VK_NULL_HANDLE);
		for (VkDescriptorSet &descriptor_set : lamb_texture_descriptors) {
			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &descriptor_set) );
		}

		// VkDescriptorSetAllocateInfo alloc_normaps_info {
		// 	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		// 	.descriptorPool = texture_descriptor_pool,
		// 	.descriptorSetCount = 1,
		// 	.pSetLayouts = &objects_pipeline.set4_TEXTURE_LAMB,
		// };
		// lamb_texture_descriptors.assign(lambertian_lookup_tex.size(), VK_NULL_HANDLE);
		// for (VkDescriptorSet &descriptor_set : lamb_texture_descriptors) {
		// 	VK( vkAllocateDescriptorSets(rtg.device, &alloc_normaps_info, &descriptor_set) );
		// }

		// TODO: add in a default
		VkImageView default_2DView = material_views.empty() ? VK_NULL_HANDLE : material_views[0];
		VkImageView default_cubeView = cube_texture_views.empty() ? VK_NULL_HANDLE : cube_texture_views[0];
		std::cout << "cube size: " << cube_texture_views.size() << std::endl;
		VkImageView default_normalView = material_views[1];
		std::cout << "lamb lookup size: " << lambertian_lookup_views.size() << std::endl;
		VkImageView lambertian_lookup_view = lambertian_lookup_views.empty() ? VK_NULL_HANDLE : lambertian_lookup_views[0];

		uint32_t total_num_texes = obj_materials.size() + cube_textures.size() + lambertian_lookup_tex.size();
		// write descriptors for textures
		std::vector< VkDescriptorImageInfo > infos(4*total_num_texes);
		std::vector< VkWriteDescriptorSet > writes(4*total_num_texes);
		size_t i = 0;
		//std::cout << "now handling 2D textures " << std::endl;
		for (auto &currMat : obj_materials) {
			// i = &image - &textures_in_use[0];
			//std::cout << "error in textures" << std::endl;
			// 2D binding 0
			infos[4*i] = (VkDescriptorImageInfo{
				.sampler = material_sampler,
				.imageView = material_views[currMat.texture_idx],
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});
			// cube binding 1 (dummy here bc 2d)
			infos[4*i+1] = (VkDescriptorImageInfo{
				.sampler = cube_texture_sampler,
				// .imageView = cube_texture_views[1],
				.imageView = default_cubeView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});

			infos[4*i+2] = (VkDescriptorImageInfo{
				.sampler = cube_texture_sampler,
				.imageView = lambertian_lookup_view,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});

			infos[4*i+3] = (VkDescriptorImageInfo{
				.sampler = material_sampler,
				.imageView = material_views[currMat.normal_map_tex_idx],
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});

			writes[4*i] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material_descriptors[i],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i],
			});

			writes[4*i+1] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material_descriptors[i],
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i+1],
			});

			writes[4*i+2] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material_descriptors[i],
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i+2],
			});

			writes[4*i+3] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material_descriptors[i],
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i+3],
			});
			++i;
		}
		
		// add the same for the cube textures
		//std::cout << "now handling cube textures " << std::endl;
		size_t old_i = i;
		for (Helpers::AllocatedImage const &image : cube_textures) {
			(void) image;
		
			//std::cout << "current i: " << i << std::endl;
			// 2d binding 0 (dummy here bc cube)
			infos[4*i] = (VkDescriptorImageInfo{
				.sampler = material_sampler,
				.imageView = default_2DView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});
			// cube binding 1 
			infos[4*i+1] = (VkDescriptorImageInfo{
				.sampler = cube_texture_sampler,
				.imageView = cube_texture_views[i - old_i],
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});

			infos[4*i+2] = (VkDescriptorImageInfo{
				.sampler = cube_texture_sampler,
				.imageView = lambertian_lookup_view,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});

			infos[4*i+3] = (VkDescriptorImageInfo{
				.sampler = material_sampler,
				.imageView = default_normalView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});

			writes[4*i] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = cube_texture_descriptors[i - old_i],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i],
			});

			writes[4*i+1] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = cube_texture_descriptors[i - old_i],
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i+1],
			});

			writes[4*i+2] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = cube_texture_descriptors[i - old_i],
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i+2],
			});

			writes[4*i+3] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = cube_texture_descriptors[i - old_i],
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i+3],
			});

			++i;
		}
		std::cout << "about to do lamb lookups" << std::endl;
		old_i = i;
		for (Helpers::AllocatedImage const &image : lambertian_lookup_tex) {
			(void) image;
		
			std::cout << "current i - old_i: " << i - old_i << std::endl;
			// 2d binding 0 (dummy here bc cube)
			infos[4*i] = (VkDescriptorImageInfo{
				.sampler = material_sampler,
				.imageView = default_2DView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});
			// cube binding 1 
			infos[4*i+1] = (VkDescriptorImageInfo{
				.sampler = cube_texture_sampler,
				.imageView = default_cubeView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});

			infos[4*i+2] = (VkDescriptorImageInfo{
				.sampler = cube_texture_sampler,
				.imageView = lambertian_lookup_view,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});

			infos[4*i+3] = (VkDescriptorImageInfo{
				.sampler = cube_texture_sampler,
				.imageView = default_normalView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			});

			writes[4*i] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lamb_texture_descriptors[i - old_i],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i],
			});

			writes[4*i+1] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lamb_texture_descriptors[i - old_i],
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i+1],
			});

			writes[4*i+2] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lamb_texture_descriptors[i - old_i],
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i+2],
			});

			writes[4*i+3] = (VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lamb_texture_descriptors[i - old_i],
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[4*i+3],
			});

			++i;
		}

		vkUpdateDescriptorSets(rtg.device, 
		                       uint32_t(writes.size()), writes.data(),
							   0, nullptr);
		std::cout << "successfully updated descriptor sets" << std::endl;
	}

	{ // add in default lights 
		if (rtg.scene.lights.empty()) {
			// world.SKY_DIRECTION.x = 0.0f;
			// world.SKY_DIRECTION.y = 0.0f;
			// world.SKY_DIRECTION.z = 1.0f;

			// world.SKY_ENERGY.r = 0.1f;
			// world.SKY_ENERGY.g = 0.1f;
			// world.SKY_ENERGY.b = 0.2f;

			// world.SUN_DIRECTION.x = 6.0f / 23.0f;
			// world.SUN_DIRECTION.y = 13.0f / 23.0f;
			// world.SUN_DIRECTION.z = 18.0f / 23.0f;

			// world.SUN_ENERGY.r = 1.0f;
			// world.SUN_ENERGY.g = 1.0f;
			// world.SUN_ENERGY.b = 0.9f;

			sunlight_insts.emplace_back(ObjectsPipeline::Sun_Light{
				.angle_w_pad{
					.angle = 0.0f,
				},
				.SUN_DIRECTION{	
					.x = 6.0f / 23.0f,
					.y = 13.0f / 23.0f,
					.z = 18.0f / 23.0f,
				},
				.SUN_ENERGY{
					.r = 1.0f,
					.g = 1.0f,
					.b = 0.9f,
				}
			});
		}
	}

	{ // traverse the scene:
		//std::cout << "number of object instances before: " << object_instances.size() << std::endl;
		traverse_scene(rtg.scene, object_instances);
		std::cout << "number of object instances after: " << object_instances.size() << std::endl;
		total_cameras = rtg.scene.cameras.size();
		std::cout << "number of scene cameras after: " << total_cameras << std::endl;
	}

	{ // map all the cameras:
		int camera_idx = 0;
		for (auto camera : rtg.scene.cameras) {
			map_camera_idxs_to_string.insert({camera_idx, camera.second.name});
			++camera_idx;
		}

		if (rtg.userEnteredCamera != "") {
			camera_mode = CameraMode::Scene;
			currCamera = scene_cameras[rtg.userEnteredCamera];

			mat4 persp = perspective(
					(currCamera).fov,
					(currCamera).aspect,
					(currCamera).near,
					(currCamera).far
				);

			camera_aspect = (currCamera).aspect;
			
			CLIP_FROM_WORLD = convert_back_to_mymat4(convert_to_glm_mat4(persp) * glm::inverse(convert_to_glm_mat4((currCamera).transform)));
		}

		else {
			std::cout << "camera aspect: " << camera_aspect << std::endl;
			camera_mode = CameraMode::Free;
		}
	}
}

Tutorial::~Tutorial() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in Tutorial::~Tutorial [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	if (texture_descriptor_pool) {
		vkDestroyDescriptorPool(rtg.device, texture_descriptor_pool, nullptr);
		texture_descriptor_pool = nullptr;

		// frees descriptor sets allocated from pool 
		material_descriptors.clear();
	}

	if (material_sampler) {
		vkDestroySampler(rtg.device, material_sampler, nullptr);
		material_sampler = VK_NULL_HANDLE;
	}	

	if (cube_texture_sampler) {
		vkDestroySampler(rtg.device, cube_texture_sampler, nullptr);
		cube_texture_sampler = VK_NULL_HANDLE;
	}

	for (VkImageView &view : material_views) {
		vkDestroyImageView(rtg.device, view, nullptr);
		view = VK_NULL_HANDLE;
	}
	material_views.clear();

	for (VkImageView &view : cube_texture_views) {
		vkDestroyImageView(rtg.device, view, nullptr);
		view = VK_NULL_HANDLE;
	}
	cube_texture_views.clear();

	for (VkImageView &view : lambertian_lookup_views) {
		vkDestroyImageView(rtg.device, view, nullptr);
		view = VK_NULL_HANDLE;
	}
	lambertian_lookup_views.clear();
	
	for (auto &texture : textures_in_use) {
		rtg.helpers.destroy_image(std::move(texture));
	}
	textures_in_use.clear();

	for (auto &texture : cube_textures) {
		rtg.helpers.destroy_image(std::move(texture));
	}
	cube_textures.clear();

	for (auto &texture : lambertian_lookup_tex) {
		rtg.helpers.destroy_image(std::move(texture));
	}
	lambertian_lookup_tex.clear();
	
	rtg.helpers.destroy_buffer(std::move(object_vertices));

	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	for (Workspace &workspace : workspaces) {

		if (workspace.command_buffer != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(rtg.device, command_pool, 1, &workspace.command_buffer);
			workspace.command_buffer = VK_NULL_HANDLE;
		}

		if (workspace.lines_vertices_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices_src));
		}

		if (workspace.lines_vertices.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices));
		}

		if (workspace.Camera_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Camera_src));
		}

		if (workspace.Camera.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Camera));
		}

		if (workspace.Sunlights_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Sunlights_src));
		}

		if (workspace.Sunlights.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Sunlights));
		}

		if (workspace.Spherelights_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Sunlights_src));
		}

		if (workspace.Spherelights.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Sunlights));
		}

		
		// World descriptors freed when pool destroyed 

		if (workspace.Transforms_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Transforms_src));
		}

		if (workspace.Transforms.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Transforms));
		}

		// transforms_descriptors free when pool is destroyed
	}
	workspaces.clear();
     
	// free the whole pool at once 
	if (descriptor_pool) {
		vkDestroyDescriptorPool(rtg.device, descriptor_pool, nullptr);
		descriptor_pool = nullptr;
		// descriptor sets also freed 
	}

	background_pipeline.destroy(rtg);
	lines_pipeline.destroy(rtg);
	objects_pipeline.destroy(rtg);

	// destroy command pool
	if (command_pool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(rtg.device, command_pool, nullptr);
		command_pool = VK_NULL_HANDLE;
	}

	if (render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, render_pass, nullptr);
		render_pass = VK_NULL_HANDLE;
	}
}

void Tutorial::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) {
	//[re]create framebuffers:
	
	// cleanup existing framebuffers (and depth image):
	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	// allocate depth image for framebuffers to share
	swapchain_depth_image = rtg.helpers.create_image(
		swapchain.extent,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped
	);

	{ // create an image view of the depth image
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_depth_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = depth_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};

		VK( vkCreateImageView(rtg.device, &create_info, nullptr, &swapchain_depth_image_view) );
	}

	// create framebufers pointing to each swapchain image view and the shared depth image view
	swapchain_framebuffers.assign(swapchain.image_views.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchain.image_views.size(); ++i) {
		std::array< VkImageView, 2 > attachments{
			swapchain.image_views[i],
			swapchain_depth_image_view,
		};

		VkFramebufferCreateInfo create_info {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = render_pass,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.width = swapchain.extent.width,
			.height = swapchain.extent.height,
			.layers = 1,
		};

		VK( vkCreateFramebuffer(rtg.device, &create_info, nullptr, &swapchain_framebuffers[i]) );
	}
}

void Tutorial::destroy_framebuffers() {
	for (VkFramebuffer &framebuffer : swapchain_framebuffers) {
		assert(framebuffer != VK_NULL_HANDLE);
		vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
		framebuffer = VK_NULL_HANDLE;
	}
	swapchain_framebuffers.clear();

	assert(swapchain_depth_image_view != VK_NULL_HANDLE);
	vkDestroyImageView(rtg.device, swapchain_depth_image_view, nullptr);
	swapchain_depth_image_view = VK_NULL_HANDLE;

	rtg.helpers.destroy_image(std::move(swapchain_depth_image));
}


void Tutorial::render(RTG &rtg_, RTG::RenderParams const &render_params) {
	// static std::unique_ptr< Timer > timer;
	// timer.reset(new Timer([](double dt){
	// 	//std::cout << "REPORT frame-time " << dt * 1000.0 << "ms" << std::endl;
	// 	std::ofstream newFile;
	// 	newFile.open("light_stress_8.txt", std::fstream::app);
	// 	newFile << dt * 1000.0 << "\n";
	// 	newFile.close();
	// }));
	//assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspaces.size());
	assert(render_params.image_index < swapchain_framebuffers.size());

	//get more convenient names for the current workspace and target framebuffer:
	Workspace &workspace = workspaces[render_params.workspace_index];
	VkFramebuffer framebuffer = swapchain_framebuffers[render_params.image_index];

	VK( vkResetCommandBuffer(workspace.command_buffer, 0) );

	{ // begin recording commands for the GPU to run 
		VkCommandBufferBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};
		VK( vkBeginCommandBuffer(workspace.command_buffer, &begin_info) );
	}

	if (!lines_vertices.empty()) { // upload lines vertices:
		// [re-]allocate lines buffers if needed:
		size_t needed_bytes = lines_vertices.size() * sizeof(lines_vertices[0]);
		//std::cout << "needed bytes" << needed_bytes << " bytes." << std::endl;
		if (workspace.lines_vertices_src.handle == VK_NULL_HANDLE || 
		    workspace.lines_vertices_src.size < needed_bytes) {
			
			// round to next multiple of 4k to avoid reallocating continuously if vertex count grows slowly :
			size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
			if (workspace.lines_vertices_src.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices_src));
			}
			if (workspace.lines_vertices.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices));
			}

			// allocate time!
			workspace.lines_vertices_src = rtg.helpers.create_buffer(
				new_bytes, 
				// going to have GPU copy from this memory
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				// host-visible memory, coherent (no special sync needed)
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				Helpers::Mapped // get pointer to memory 
			);
			workspace.lines_vertices = rtg.helpers.create_buffer(
				new_bytes,
				// going to use as vertex buffer, also going to have GPU into this memory
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				// GPU-local memory 
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				Helpers::Unmapped // don't get a pointer to this memory 
			);
			std::cout << "Re-allocated lines buffers to" << new_bytes << " bytes." << std::endl;
		}

		assert(workspace.lines_vertices_src.size == workspace.lines_vertices.size);
		assert(workspace.lines_vertices_src.size >= needed_bytes);

		// host-side copy into lines_vertices_src:
		assert(workspace.lines_vertices_src.allocation.mapped);
		std::memcpy(workspace.lines_vertices_src.allocation.data(), lines_vertices.data(), needed_bytes);

		// now GPU copies lines_vertices_src -> lines_vertices:
		VkBufferCopy copy_region {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = needed_bytes,
		};
		vkCmdCopyBuffer(workspace.command_buffer, 
						workspace.lines_vertices_src.handle,
						workspace.lines_vertices.handle,
						1, &copy_region);
	}

	if (!object_instances.empty()) { // upload object transforms:
		// [re-]allocate lines buffers if needed:
		size_t needed_bytes = object_instances.size() * sizeof(ObjectsPipeline::Transform);
		//std::cout << "needed bytes" << needed_bytes << " bytes." << std::endl;
		if (workspace.Transforms_src.handle == VK_NULL_HANDLE || 
		    workspace.Transforms_src.size < needed_bytes) {
			
			// round to next multiple of 4k to avoid reallocating continuously if vertex count grows slowly :
			size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
			if (workspace.Transforms_src.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.Transforms_src));
			}
			if (workspace.Transforms.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.Transforms));
			}

			// allocate time!
			workspace.Transforms_src = rtg.helpers.create_buffer(
				new_bytes, 
				// going to have GPU copy from this memory
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				// host-visible memory, coherent (no special sync needed)
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				Helpers::Mapped // get pointer to memory 
			);
			workspace.Transforms = rtg.helpers.create_buffer(
				new_bytes,
				// going to use as storage buffer, also going to have GPU into this memory
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				// GPU-local memory 
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				Helpers::Unmapped // don't get a pointer to this memory 
			);

			// update the descriptor set:
			VkDescriptorBufferInfo Transforms_info{
				.buffer = workspace.Transforms.handle,
				.offset = 0,
				.range = workspace.Transforms.size,
			};

			std::array< VkWriteDescriptorSet, 1 > writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.Transforms_descriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &Transforms_info,
				},
			};

			vkUpdateDescriptorSets(
				rtg.device,
				// descriptorWrites count, data
				uint32_t(writes.size()), writes.data(),
				// descriptorCopies count, data
				0, nullptr
			);

			std::cout << "Re-allocated object transforms buffers to" << new_bytes << " bytes." << std::endl;
		}

		assert(workspace.Transforms_src.size == workspace.Transforms.size);
		assert(workspace.Transforms_src.size >= needed_bytes);

		{ // host-side copy into Transforms_src:
			assert(workspace.Transforms_src.allocation.mapped);
			ObjectsPipeline::Transform *out = reinterpret_cast< ObjectsPipeline::Transform * >
															  (workspace.Transforms_src.allocation.data());
			
			for (ObjectInstance const &inst : object_instances) {
				*out = inst.transform;
				++out;
			}
		}
		
		// now GPU copies lines_vertices_src -> lines_vertices:
		VkBufferCopy copy_region {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = needed_bytes,
		};
		vkCmdCopyBuffer(workspace.command_buffer, 
						workspace.Transforms_src.handle,
						workspace.Transforms.handle,
						1, &copy_region);
	}

	if (!sunlight_insts.empty()) { // upload sunlights:
		// [re-]allocate lines buffers if needed:
		size_t needed_bytes = sunlight_insts.size() * sizeof(ObjectsPipeline::Sun_Light);
		//std::cout << "needed bytes" << needed_bytes << " bytes." << std::endl;
		if (workspace.Sunlights_src.handle == VK_NULL_HANDLE || 
		    workspace.Sunlights_src.size < needed_bytes) {
			
			// round to next multiple of 4k to avoid reallocating continuously if vertex count grows slowly :
			size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
			if (workspace.Sunlights_src.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.Sunlights_src));
			}
			if (workspace.Sunlights.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.Sunlights));
			}

			// allocate time!
			workspace.Sunlights_src = rtg.helpers.create_buffer(
				new_bytes, 
				// going to have GPU copy from this memory
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				// host-visible memory, coherent (no special sync needed)
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				Helpers::Mapped // get pointer to memory 
			);
			workspace.Sunlights = rtg.helpers.create_buffer(
				new_bytes,
				// going to use as storage buffer, also going to have GPU into this memory
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				// GPU-local memory 
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				Helpers::Unmapped // don't get a pointer to this memory 
			);

			// update the descriptor set:
			VkDescriptorBufferInfo Sunlights_info{
				.buffer = workspace.Sunlights.handle,
				.offset = 0,
				.range = workspace.Sunlights.size,
			};

			std::array< VkWriteDescriptorSet, 1 > writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.Sunlights_descriptors,
					.dstBinding = 0, // maybe need to change?
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &Sunlights_info,
				},
			};

			vkUpdateDescriptorSets(
				rtg.device,
				// descriptorWrites count, data
				uint32_t(writes.size()), writes.data(),
				// descriptorCopies count, data
				0, nullptr
			);

			std::cout << "Re-allocated object transforms buffers to" << new_bytes << " bytes." << std::endl;
		}

		assert(workspace.Sunlights_src.size == workspace.Sunlights.size);
		assert(workspace.Sunlights_src.size >= needed_bytes);

		{ // host-side copy into Sunlights_src:
			assert(workspace.Sunlights_src.allocation.mapped);
			ObjectsPipeline::Sun_Light *out = reinterpret_cast< ObjectsPipeline::Sun_Light * >
															  (workspace.Sunlights_src.allocation.data());
			
			for (ObjectsPipeline::Sun_Light const &inst : sunlight_insts) {
				*out = inst;
				++out;
			}
		}
		
		// now GPU copies lines_vertices_src -> lines_vertices:
		VkBufferCopy copy_region {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = needed_bytes,
		};
		vkCmdCopyBuffer(workspace.command_buffer, 
						workspace.Sunlights_src.handle,
						workspace.Sunlights.handle,
						1, &copy_region);
	}

	if (!spherelight_insts.empty()) { // upload sunlights:
		// [re-]allocate lines buffers if needed:
		size_t needed_bytes = spherelight_insts.size() * sizeof(ObjectsPipeline::Sphere_Light);
		//std::cout << "needed bytes" << needed_bytes << " bytes." << std::endl;
		if (workspace.Spherelights_src.handle == VK_NULL_HANDLE || 
		    workspace.Spherelights_src.size < needed_bytes) {
			
			// round to next multiple of 4k to avoid reallocating continuously if vertex count grows slowly :
			size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
			if (workspace.Spherelights_src.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.Spherelights_src));
			}
			if (workspace.Spherelights.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.Spherelights));
			}

			// allocate time!
			workspace.Spherelights_src = rtg.helpers.create_buffer(
				new_bytes, 
				// going to have GPU copy from this memory
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				// host-visible memory, coherent (no special sync needed)
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				Helpers::Mapped // get pointer to memory 
			);
			workspace.Spherelights = rtg.helpers.create_buffer(
				new_bytes,
				// going to use as storage buffer, also going to have GPU into this memory
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				// GPU-local memory 
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				Helpers::Unmapped // don't get a pointer to this memory 
			);

			// update the descriptor set:
			VkDescriptorBufferInfo Spherelights_info{
				.buffer = workspace.Spherelights.handle,
				.offset = 0,
				.range = workspace.Spherelights.size,
			};

			std::array< VkWriteDescriptorSet, 1 > writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.Spherelights_descriptors,
					.dstBinding = 0, // maybe need to change?
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &Spherelights_info,
				},
			};

			vkUpdateDescriptorSets(
				rtg.device,
				// descriptorWrites count, data
				uint32_t(writes.size()), writes.data(),
				// descriptorCopies count, data
				0, nullptr
			);

			std::cout << "Re-allocated object transforms buffers to" << new_bytes << " bytes." << std::endl;
		}

		assert(workspace.Spherelights_src.size == workspace.Spherelights.size);
		assert(workspace.Spherelights_src.size >= needed_bytes);

		{ // host-side copy into Sunlights_src:
			assert(workspace.Spherelights_src.allocation.mapped);
			ObjectsPipeline::Sphere_Light *out = reinterpret_cast< ObjectsPipeline::Sphere_Light * >
															  (workspace.Spherelights_src.allocation.data());
			
			for (ObjectsPipeline::Sphere_Light const &inst : spherelight_insts) {
				*out = inst;
				++out;
			}
		}
		
		// now GPU copies lines_vertices_src -> lines_vertices:
		VkBufferCopy copy_region {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = needed_bytes,
		};
		vkCmdCopyBuffer(workspace.command_buffer, 
						workspace.Spherelights_src.handle,
						workspace.Spherelights.handle,
						1, &copy_region);
	}

	if (!spotlight_insts.empty()) { // upload sunlights:
		// [re-]allocate lines buffers if needed:
		size_t needed_bytes = spotlight_insts.size() * sizeof(ObjectsPipeline::Spot_Light);
		//std::cout << "needed bytes" << needed_bytes << " bytes." << std::endl;
		if (workspace.Spotlights_src.handle == VK_NULL_HANDLE || 
		    workspace.Spotlights_src.size < needed_bytes) {
			
			// round to next multiple of 4k to avoid reallocating continuously if vertex count grows slowly :
			size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
			if (workspace.Spotlights_src.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.Spotlights_src));
			}
			if (workspace.Spotlights.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.Spotlights));
			}

			// allocate time!
			workspace.Spotlights_src = rtg.helpers.create_buffer(
				new_bytes, 
				// going to have GPU copy from this memory
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				// host-visible memory, coherent (no special sync needed)
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				Helpers::Mapped // get pointer to memory 
			);
			workspace.Spotlights = rtg.helpers.create_buffer(
				new_bytes,
				// going to use as storage buffer, also going to have GPU into this memory
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				// GPU-local memory 
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				Helpers::Unmapped // don't get a pointer to this memory 
			);

			// update the descriptor set:
			VkDescriptorBufferInfo Spotlights_info{
				.buffer = workspace.Spotlights.handle,
				.offset = 0,
				.range = workspace.Spotlights.size,
			};

			std::array< VkWriteDescriptorSet, 1 > writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.Spotlights_descriptors,
					.dstBinding = 0, // maybe need to change?
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &Spotlights_info,
				},
			};

			vkUpdateDescriptorSets(
				rtg.device,
				// descriptorWrites count, data
				uint32_t(writes.size()), writes.data(),
				// descriptorCopies count, data
				0, nullptr
			);

			std::cout << "Re-allocated object transforms buffers to" << new_bytes << " bytes." << std::endl;
		}

		assert(workspace.Spotlights_src.size == workspace.Spotlights.size);
		assert(workspace.Spotlights_src.size >= needed_bytes);

		{ // host-side copy into Sunlights_src:
			assert(workspace.Spotlights_src.allocation.mapped);
			ObjectsPipeline::Spot_Light *out = reinterpret_cast< ObjectsPipeline::Spot_Light * >
															  (workspace.Spotlights_src.allocation.data());
			
			for (ObjectsPipeline::Spot_Light const &inst : spotlight_insts) {
				*out = inst;
				++out;
			}
		}
		
		// now GPU copies lines_vertices_src -> lines_vertices:
		VkBufferCopy copy_region {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = needed_bytes,
		};
		vkCmdCopyBuffer(workspace.command_buffer, 
						workspace.Spotlights_src.handle,
						workspace.Spotlights.handle,
						1, &copy_region);
	}

	{ // upload camera info:
		LinesPipeline::Camera camera {
			.CLIP_FROM_WORLD = CLIP_FROM_WORLD
		};

		assert(workspace.Camera_src.size == sizeof(camera));

		// host side copy into Camera_src:
		memcpy(workspace.Camera_src.allocation.data(), &camera, sizeof(camera));

		// add a device-side copy from Camera_src -> Camera:
		assert(workspace.Camera_src.size == workspace.Camera.size);
		VkBufferCopy copy_region {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = workspace.Camera_src.size,
		};

		vkCmdCopyBuffer(workspace.command_buffer, 
						workspace.Camera_src.handle,
						workspace.Camera.handle,
						1,
						&copy_region);
	}

	// { // upload world info:
	// 	assert(workspace.Camera_src.size == sizeof(world));

	// 	// host-side copy into world_src:
	// 	memcpy(workspace.World_src.allocation.data(), &world, sizeof(world));

	// 	// add device-side copy from World_src -> World:
	// 	assert(workspace.World_src.size == workspace.World.size);
	// 	VkBufferCopy copy_region{
	// 		.srcOffset = 0,
	// 		.dstOffset = 0,
	// 		.size = workspace.World_src.size,
	// 	};

	// 	vkCmdCopyBuffer(workspace.command_buffer, workspace.World_src.handle, workspace.World.handle, 1, &copy_region);
	// }

	{ // make sure copies complete before rendering
		VkMemoryBarrier memory_barrier {
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		};

		vkCmdPipelineBarrier( workspace.command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, //srcStageMask
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, // dstStageMask
			0, // dependencyFlags
			1, &memory_barrier, // memoryBarriers
			0, nullptr, // bufferMemoryBarriers
			0, nullptr // imageMemoryBarriers
		);
	}

	{ // render pass part 
		std::array< VkClearValue, 2 > clear_values {
			VkClearValue{ .color{ .float32{0.2, 0.5f, 1.0f, 0.8f}}},
			VkClearValue{ .depthStencil { .depth = 1.0f, .stencil = 0}},
		};

		VkRenderPassBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = render_pass,
			// references the attachments used in a render pass 
			.framebuffer = framebuffer,
			// tells the pixel area that is being rendered to
			.renderArea {
				.offset = {.x = 0, .y = 0},
				.extent = rtg.swapchain_extent,
			},
			.clearValueCount = uint32_t(clear_values.size()),
			// color buffer and depth buffer loaded by begin cleared to the below vals
			.pClearValues = clear_values.data(),
		};

		vkCmdBeginRenderPass (workspace.command_buffer, 
			                  &begin_info, 
			                  VK_SUBPASS_CONTENTS_INLINE);

		// run pipelines
		
		//float scissorWidth = 0.0f;
		float correctWidth = camera_aspect * rtg.swapchain_extent.height;
		//std::cout << correctWidth << " <- this is the correct width" << std::endl;
		float correctHeight = rtg.swapchain_extent.width / camera_aspect;

		float currAspectRatio = rtg.swapchain_extent.width / rtg.swapchain_extent.height;
		
		//std::cout << window_aspect << " <- this is the window_aspect" << std::endl;
		// figured out with Enrique 
		{ // configure viewport transform :
			// how device coords map to window coords
			VkViewport viewport {
				.x = 0.0f, 
				.y = 0.0f, 
				// ensures that it covers the whole swapchain img
				// .width = float(rtg.swapchain_extent.height),
				.width = float(rtg.swapchain_extent.width),
				.height = float(rtg.swapchain_extent.height),
				.minDepth = 0.0f,
				.maxDepth = 1.0f,
			};
			// worked with Enrique to do scissor rects 
			if (currAspectRatio > camera_aspect) {
				float width_diff = rtg.swapchain_extent.width - correctWidth;
				float scissorWidth = width_diff / 2.0f;
				viewport.width = correctWidth;
				//std::cout << "scissor width " << scissorWidth << std::endl;
				viewport.x = scissorWidth;
			}
			
			else {
				float height_diff = rtg.swapchain_extent.height - correctHeight;
				float scissorHeight = height_diff / 2.0f;
				viewport.height = correctHeight;
				viewport.y = scissorHeight;
				//std::cout << viewport.y << "<- viewport.y" << std::endl;
			}

			vkCmdSetViewport(workspace.command_buffer, 0, 1, &viewport);

			// set scissor rectangle
			//subset of screen that gets drawn to 
			VkRect2D scissor {
				.offset = {.x = int32_t(viewport.x), 
					       .y = int32_t(viewport.y)},
				.extent = {.width = uint32_t(viewport.width),
				           .height = uint32_t(viewport.height)}, // ensures that it covers the whole swapchain img
			};
			// why does a vector here not work!!!
			vkCmdSetScissor(workspace.command_buffer, 0, 1, &scissor);
		}

		{ // set a different box color
			VkClearAttachment clearAttachInfo {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.colorAttachment = 0,
				.clearValue.color = {{0.482, 0.49, 0.502, 1.0}},
			};

			VkClearRect clearRectInfo {
				.rect = {.extent = rtg.swapchain_extent, 
				         .offset = {.x = 0, .y = 0}},
				.baseArrayLayer = 0,
				.layerCount = 1,
			};

			vkCmdClearAttachments(workspace.command_buffer, 1, &clearAttachInfo,
				                                            1, &clearRectInfo);

		}

		if (!spotlight_insts.empty()) { // draw with shadows pipeline 
			

			vkCmdBindPipeline(workspace.command_buffer,
			                  VK_PIPELINE_BIND_POINT_GRAPHICS, 
							  shadows_pipeline.handle);
		

			for (auto const &spotlight : spotlight_insts) {
				{ // push light from world matrix 
					ShadowsPipeline::Push push {
						.LIGHT_FROM_WORLD = perspective(spotlight.SPOT_OUTER_AND_INNER_LIM.outer * 2.0f, 
							                1.0f, 
											0.01f, 
											spotlight.SPOT_POWER_AND_LIMIT.limit),
					};

					(void) push;
				}
			}

			// vkCmdDraw();
		}

		{ // draw with the background pipeline :
			vkCmdBindPipeline(workspace.command_buffer, 
				              VK_PIPELINE_BIND_POINT_GRAPHICS, 
							  background_pipeline.handle);
			{ // push time
				BackgroundPipeline::Push push {
					.time = float(time),
				};
				vkCmdPushConstants(workspace.command_buffer, 
								   background_pipeline.layout, 
								   VK_SHADER_STAGE_FRAGMENT_BIT, 
								   0, sizeof(push), &push);
			}

			vkCmdDraw(workspace.command_buffer, 3, 1, 0, 0);
		}

		{ // draw with lines pipeline
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lines_pipeline.handle);
			
			{ // use lines_vertices (offset 0) as vertex buffer bining 0:
				std::array< VkBuffer, 1 > vertex_buffers{ workspace.lines_vertices.handle};
				std::array< VkDeviceSize, 1 > offsets{ 0 };
				vkCmdBindVertexBuffers(workspace.command_buffer, 0, 
					                   uint32_t(vertex_buffers.size()), 
									   vertex_buffers.data(),
									   offsets.data());
			}

			{ // bind Camera descriptor setß
				std::array< VkDescriptorSet, 1 > descriptor_sets {
					workspace.Camera_descriptors, // 0: Camera
				};

				vkCmdBindDescriptorSets(
					workspace.command_buffer, // command buffer
					VK_PIPELINE_BIND_POINT_GRAPHICS, // pipeline bind point
					lines_pipeline.layout, // pipeline layout
					0,  // first set
					// descriptor sets count,         ptr
					uint32_t(descriptor_sets.size()), descriptor_sets.data(), 
					// dynamic sets count,         ptr
					0,                             nullptr
				);
			}       

			// actually draw the line vertices 
			vkCmdDraw(workspace.command_buffer, uint32_t(lines_vertices.size()), 1, 0, 0);

		}

		if (!object_instances.empty()) { // draw with objects pipeline
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objects_pipeline.handle);
			
			{ // use object_vertices (offset 0) as vertex buffer binding 0:
				std::array< VkBuffer, 1 > vertex_buffers{ object_vertices.handle };
				std::array< VkDeviceSize, 1 > offsets{ 0 };
				vkCmdBindVertexBuffers(workspace.command_buffer, 0, 
					                   uint32_t(vertex_buffers.size()), 
									   vertex_buffers.data(), 
									   offsets.data());
			}

			{ // bind Sun, Sphere, and Transforms descriptor set:
				std::array< VkDescriptorSet, 4 > descriptor_sets {
					workspace.Sunlights_descriptors, // 0: Sun
					workspace.Transforms_descriptors, // 1: Transforms
					workspace.Spherelights_descriptors, // 2: Spheres
					workspace.Spotlights_descriptors, // 3: Spotlights 
				};

				vkCmdBindDescriptorSets(
					workspace.command_buffer, // command buffer
					VK_PIPELINE_BIND_POINT_GRAPHICS, // pipeline bind point
					objects_pipeline.layout, // pipeline layout
					0, // first set
					// descriptor sets count, ptr
					uint32_t(descriptor_sets.size()), descriptor_sets.data(),
					// dynamic offsets count, ptr
					0, nullptr
				);
			}

			
			ObjectsPipeline::Push push {
				.EYE_vec = vec3{0.0f, 0.0f, 0.0f},
				.tex_type = 0,
				.exposure = rtg.exposure,
			};

			// camera descriptor still bound but unused

			// draw all instances:
			for (ObjectInstance const &inst : object_instances) {
				uint32_t index = uint32_t(&inst - &object_instances[0]);
				//std::cout << "binding textures" << std::endl;
				//bind texture descriptor set:
				if (inst.texture_type == 0) {

					//VkDescriptorSet *descriptorArray{texture_descriptors[inst.texture], }
					//std::cout << "sending out a 2D texture bind" << std::endl;
					
					vkCmdBindDescriptorSets(
						workspace.command_buffer, // command buffer
						VK_PIPELINE_BIND_POINT_GRAPHICS, // pipeline bind point
						objects_pipeline.layout, // pipeline layout
						4, // fourth set
						// descriptor sets count, ptr
						1, &material_descriptors[inst.material],
						// dynamic offsets count, ptr
						0, nullptr
					);

					push.tex_type = 0;

					// switch out the textype based on which texture it is
					vkCmdPushConstants(workspace.command_buffer, 
									objects_pipeline.layout, 
									VK_SHADER_STAGE_FRAGMENT_BIT, 
									0, sizeof(push), &push);
				}

				else if (inst.texture_type == 1) {
					// std::cout << "sending out a cube texture bind" << std::endl;
					// std::cout << inst.cubeTexture << std::endl;
					// std::cout << cube_texture_descriptors.size() << std::endl;
					vkCmdBindDescriptorSets(
						workspace.command_buffer, // command buffer
						VK_PIPELINE_BIND_POINT_GRAPHICS, // pipeline bind point
						objects_pipeline.layout, // pipeline layout
						4, // fourth set
						// descriptor sets count, ptr
						1, &material_descriptors[inst.material],
						// dynamic offsets count, ptr
						0, nullptr
					);

					push.tex_type = 1;

					// switch out the textype based on which texture it is
					vkCmdPushConstants(workspace.command_buffer, 
									objects_pipeline.layout, 
									VK_SHADER_STAGE_FRAGMENT_BIT, 
									0, sizeof(push), &push);
				}

				else if (inst.texture_type == 2) {
					// std::cout << "sending out a mirror texture bind" << std::endl;
					// std::cout << inst.cubeTexture << std::endl;
					// std::cout << cube_texture_descriptors.size() << std::endl;
					vkCmdBindDescriptorSets(
						workspace.command_buffer, // command buffer
						VK_PIPELINE_BIND_POINT_GRAPHICS, // pipeline bind point
						objects_pipeline.layout, // pipeline layout
						4, // fourth set
						// descriptor sets count, ptr
						1, &material_descriptors[inst.material],
						// dynamic offsets count, ptr
						0, nullptr
					);

					push.tex_type = 2;
					// come back to this and make sure the scene cameras work too!
					push.EYE_vec = get_eye(currCamera.target, 
					                       currCamera.azimuth,
										   currCamera.elevation,
										   currCamera.radius);
					// std::cout << "here is the current eye" << std::endl;
					// print_vector_3x1(push.EYE_vec);

					// switch out the textype based on which texture it is
					vkCmdPushConstants(workspace.command_buffer, 
									objects_pipeline.layout, 
									VK_SHADER_STAGE_FRAGMENT_BIT, 
									0, sizeof(push), &push);
				}

				vkCmdDraw(workspace.command_buffer, 
					      inst.vertices.count, 
						  1, 
						  inst.vertices.first, 
						  index);
			}

		}
		vkCmdEndRenderPass (workspace.command_buffer);
	}

	// end recording
	VK( vkEndCommandBuffer(workspace.command_buffer) );

	{ //submit `workspace.command buffer` for the GPU to run:
		// signalled when image done presented and ready to render to
		std::array< VkSemaphore, 1 > wait_semaphores{
			render_params.image_available 
		};

		std::array< VkPipelineStageFlags, 1 > wait_stages{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		};
		static_assert(wait_semaphores.size() == wait_stages.size(), "every semaphore needs a stage");

		// signalled when rendering work in a whole batch is done
		std::array< VkSemaphore, 1 > signal_semaphores{
			render_params.image_done
		};

		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = uint32_t(wait_semaphores.size()),
			.pWaitSemaphores = wait_semaphores.data(),
			.pWaitDstStageMask = wait_stages.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &workspace.command_buffer,
			.signalSemaphoreCount = uint32_t(signal_semaphores.size()),
			.pSignalSemaphores = signal_semaphores.data(),
		};
		// workspace_available to make sure nothing is using members of workspace when workspace is recycled for a new render call
		VK( vkQueueSubmit(rtg.graphics_queue, 1, &submit_info, render_params.workspace_available) );

	}
}

void Tutorial::update(float dt) {
	float longest_cycle = 0.0f;
	for (auto &driver : rtg.scene.drivers) {
		float animation_cycle_length = driver.times.back();
		if (animation_cycle_length > longest_cycle) {
			longest_cycle = animation_cycle_length;
		}
	} 

	time = std::fmod(time + dt, longest_cycle);
	//std::cout << "camera_mode: " << int(camera_mode) << std::endl;
	if (!rtg.scene.drivers.empty()) {
		for (auto &driver : rtg.scene.drivers) {

			// S72::Node currNode = driver.node;
			
			//size_t totalTimes = driver.times.size();
			if (time < driver.times[driver.keyFrameNum]) {
				driver.keyFrameNum = 0;
			}

			// make sure the keyframes stay within the bounds of the times std:vector 
			while (driver.keyFrameNum + 1 < driver.times.size() && driver.times[driver.keyFrameNum + 1] <= time) {
				//std::cout << "updating keyframe!!" << std::endl;
				driver.keyFrameNum++;
			}

			uint32_t currKeyFrame = driver.keyFrameNum;
			uint32_t nextKeyFrame = driver.keyFrameNum + 1;
			//std::cout << "time: " << time << std::endl;
			// std::cout << dt << std::endl;
			float timeInt = 0.0f;

			// reset back to the beginning 
			if (nextKeyFrame > driver.times.size()) {
				nextKeyFrame = currKeyFrame;
				timeInt = 0.0f;
			}
			
			
			else {
				float currTime = driver.times[currKeyFrame];
				float nextTime = driver.times[nextKeyFrame];
				float timeDiff = nextTime - currTime;
				// check if there is a time interval between the current and next keyframe 
				timeInt = (timeDiff > 0.00001f) ? ((time - currTime) / timeDiff) : 0.0f;

				// clamp it for interpolation 
				if (timeInt > 1.0f) timeInt = 1.0f;
			}

			if (driver.interpolation == S72::Driver::Interpolation::STEP) {
				if (driver.channel == S72::Driver::Channel::translation || 
					driver.channel == S72::Driver::Channel::scale) {
					//std::cout << "--------------- NODE NAME: " << driver.node.name << " -------------------" << std::endl;
					//std::cout << "--------------- old translation" << std::endl;
					//print_matrix4x4(object_instances[currNode.obj_index].transform.WORLD_FROM_LOCAL);
					
					vec3 firstVal = vec3{driver.values[currKeyFrame * 3], 
										driver.values[currKeyFrame * 3 + 1],
										driver.values[currKeyFrame * 3 + 2]};

					//std::cout << "---------------- first translation" << std::endl;
											
				
			
					//mat4 newTransMat = get_translation_matrix(LERPtrans);
					if (driver.channel == S72::Driver::Channel::translation) 
						driver.node.translation = firstVal;

					else driver.node.scale = firstVal;
					

					//std::cout << "---------------- translation matrix" << std::endl;
					//print_matrix4x4(newTransMat);
					
				}

				else if (driver.channel == S72::Driver::Channel::rotation) {
					vec4 firstVal = vec4{driver.values[currKeyFrame * 4], 
										driver.values[currKeyFrame * 4 + 1],
										driver.values[currKeyFrame * 4 + 2],
										driver.values[currKeyFrame * 4 + 3]};
											
					
					driver.node.rotation = firstVal;
									
				}
			}	
			
			else if (driver.interpolation == S72::Driver::Interpolation::LINEAR) {
				if (driver.channel == S72::Driver::Channel::translation || 
					driver.channel == S72::Driver::Channel::scale) {
					//std::cout << "--------------- NODE NAME: " << driver.node.name << " -------------------" << std::endl;
					//std::cout << "--------------- old translation" << std::endl;
					//print_matrix4x4(object_instances[currNode.obj_index].transform.WORLD_FROM_LOCAL);
					
					vec3 firstVal = vec3{driver.values[currKeyFrame * 3], 
										driver.values[currKeyFrame * 3 + 1],
										driver.values[currKeyFrame * 3 + 2]};

					//std::cout << "---------------- first translation" << std::endl;
											
					vec3 secVal   = vec3{driver.values[nextKeyFrame * 3], 
										driver.values[nextKeyFrame * 3 + 1],
										driver.values[nextKeyFrame * 3 + 2]};	
					
					//std::cout << "---------------- second translation" << std::endl;

					
					vec3 LERPval = firstVal + (secVal - firstVal) * timeInt;
			
					//mat4 newTransMat = get_translation_matrix(LERPtrans);
					if (driver.channel == S72::Driver::Channel::translation) 
						driver.node.translation = LERPval;

					else driver.node.scale = LERPval;
					

					//std::cout << "---------------- translation matrix" << std::endl;
					//print_matrix4x4(newTransMat);
					
				}

				else if (driver.channel == S72::Driver::Channel::rotation) {
					vec4 firstVal = vec4{driver.values[currKeyFrame * 4], 
										driver.values[currKeyFrame * 4 + 1],
										driver.values[currKeyFrame * 4 + 2],
										driver.values[currKeyFrame * 4 + 3]};
											
					vec4 secVal   = vec4{driver.values[nextKeyFrame * 4], 
										driver.values[nextKeyFrame * 4 + 1],
										driver.values[nextKeyFrame * 4 + 2],
										driver.values[nextKeyFrame * 4 + 3]};	
					

					
					vec4 LERPval = firstVal + (secVal - firstVal) * timeInt;

					driver.node.rotation = LERPval;
									
				}
				
			}	

			else if (driver.interpolation == S72::Driver::Interpolation::SLERP) {
				if (driver.channel == S72::Driver::Channel::rotation) {
					vec4 firstVal = vec4{driver.values[currKeyFrame * 4], 
										driver.values[currKeyFrame * 4 + 1],
										driver.values[currKeyFrame * 4 + 2],
										driver.values[currKeyFrame * 4 + 3]};
											
					vec4 secVal   = vec4{driver.values[nextKeyFrame * 4], 
										driver.values[nextKeyFrame * 4 + 1],
										driver.values[nextKeyFrame * 4 + 2],
										driver.values[nextKeyFrame * 4 + 3]};

					if (dot(firstVal, secVal) < 0.0f) {
						secVal = vec4{-secVal[0], -secVal[1], -secVal[2], -secVal[3]};
					}

					if (1.0f - dot(firstVal, secVal) < std::numeric_limits<float>::epsilon()) {
						vec4 LERPval = firstVal + (secVal - firstVal) * timeInt;
						driver.node.rotation = LERPval;
					}

					else {
						float angle = acos(dot(firstVal, secVal));
						vec4 SLERPval = sin((1 - timeInt) * angle) / sin(angle) * firstVal +
										sin((timeInt) * angle) / sin(angle) * secVal;  

						//print_vector_4x1(SLERPval);
						driver.node.rotation = SLERPval;
					}
				}
			}	
			
		}
		traverse_scene(rtg.scene, object_instances);
	}
	
	if (camera_mode == CameraMode::Scene) { 
		//std::cout << currCamera.aspect << std::endl;
		currCamera = scene_cameras[map_camera_idxs_to_string[scene_camera_index]];
		camera_aspect = (currCamera).aspect;

		update_camera(currCamera);
	}

	else if (camera_mode == CameraMode::Free) {
		mat4 persp = perspective(
			free_camera.fov,
			free_camera.aspect, // aspect
			free_camera.near,
			free_camera.far
		);
		mat4 orbit_matrix = orbit(
			free_camera.target, 
			free_camera.azimuth, 
			free_camera.elevation, 
			free_camera.radius
		);
	
		camera_aspect = free_camera.aspect;
		CLIP_FROM_WORLD = persp * orbit_matrix;

		free_camera.currClipFromWorld = CLIP_FROM_WORLD;
		currCamera = free_camera;
		currCamera.currClipFromWorld = CLIP_FROM_WORLD;
		// std::cout << "CFW ------------------" << std::endl;
		// print_matrix_helper(CLIP_FROM_WORLD);
		// std::cout << "This is the free camera" << std::endl;
		// print_matrix4x4(currCamera.currClipFromWorld);
	}

	else if (camera_mode == CameraMode::Debug) {
		mat4 persp = perspective(
			debug_camera.fov,
			debug_camera.aspect, // aspect
			debug_camera.near,
			debug_camera.far
		);
		mat4 orbit_matrix = orbit(
			debug_camera.target, 
			debug_camera.azimuth, 
			debug_camera.elevation, 
			debug_camera.radius
		);

		camera_aspect = free_camera.aspect;

		CLIP_FROM_WORLD = persp * orbit_matrix;

		debug_camera.currClipFromWorld = CLIP_FROM_WORLD;
		currCamera = debug_camera;
		currCamera.currClipFromWorld = CLIP_FROM_WORLD;
		
		// std::cout << "CFW ------------------" << std::endl;
		// print_matrix_helper(CLIP_FROM_WORLD);
	}

	else {
		assert(0 && "only three camera modes");
	}
	
	{ // make some objects:
		//std::cout << "Curr num of objs: " << object_instances.size() << std::endl;

		lines_vertices.clear();

		for (ObjectInstance &obj : object_instances) {
			//std::cout << " curr object: " << obj.vertices.first << std::endl;
			obj.transform.CLIP_FROM_LOCAL = CLIP_FROM_WORLD * obj.transform.WORLD_FROM_LOCAL;

			if (camera_mode == CameraMode::Debug) {
				lines_vertices.insert(lines_vertices.end(), obj.bbox_lines.begin(), obj.bbox_lines.end());
			}
			
			else {
				lines_vertices.emplace_back(PosColVertex {
					.Position{.x = 0.0, .y = 0.0f, .z = 0.0f},
					.Color{.r = 0x44, .g = 0x00, .b = 0xff, .a = 0xff}
				});
			}
			//std::cout << " this is the new clip from local " << std::endl;
			//print_matrix4x4(obj.transform.CLIP_FROM_LOCAL);
		}

		if (camera_mode == CameraMode::Debug) {
			std::vector< LinesPipeline::Vertex > temp_frustum_lines_vertices;
			mat4 worldFromClip = convert_back_to_mymat4(glm::inverse(convert_to_glm_mat4((prevCamera).currClipFromWorld)));
			draw_bbox(temp_frustum_lines_vertices, vec3{-1,-1,0}, vec3{1,1,1}, worldFromClip, true);
			
			// for (auto &vertex : temp_frustum_lines_vertices) {
			
			// 	vertex.Position.y = -vertex.Position.y;
			// }

			lines_vertices.insert(lines_vertices.end(), temp_frustum_lines_vertices.begin(), temp_frustum_lines_vertices.end());
		}
	}
}

mat4 Tutorial::get_node_xform(S72::Node *node) {
    mat4 transMat = get_translation_matrix(node->translation);
    mat4 rotMat = get_rotation_matrix(node->rotation);
    mat4 scaleMat = get_scale_matrix(node->scale);

    return get_parent_from_local(transMat, rotMat, scaleMat);
}

void Tutorial::traverse_scene(S72 &scene, std::vector< Tutorial::ObjectInstance > &scene_objects) {
    // std::vector< S72::Node * > root_nodes = scene.scene.roots;
    scene_objects.clear();
	scene_cameras.clear();
	sunlight_insts.clear();
	spherelight_insts.clear();
	spotlight_insts.clear();

    for (S72::Node *root : scene.scene.roots) {
        traverse_root(root, scene_objects);
    }
}

void Tutorial::draw_bbox(std::vector< LinesPipeline::Vertex > &lines_buff, vec3 box_min, vec3 box_max, mat4 curr_xform, bool isFrustum) {
	float box_depth = std::abs(box_max[2] - box_min[2]);
	float box_height = std::abs(box_max[1] - box_min[1]);
	float box_width = std::abs(box_max[0] - box_min[0]);

	vec4 color;

	if (isFrustum) color = vec4{255, 112, 46, 0xff}; 
	else color = vec4{70, 173, 242, 0xff};

	// first square 
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1], .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1], .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1], .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1] + box_height, .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1] + box_height, .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1] + box_height, .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1] + box_height, .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1], .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});

    // second square 
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1], .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1], .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1], .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1] + box_height, .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1] + box_height, .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1] + box_height, .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1] + box_height, .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1], .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});

	// lines in between
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1], .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1], .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1], .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1], .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1] + box_height, .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0] + box_width, .y = box_min[1] + box_height, .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1] + box_height, .z = box_min[2]},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
	lines_buff.emplace_back(PosColVertex {
				.Position{.x = box_min[0], .y = box_min[1] + box_height, .z = box_min[2] + box_depth},
				.Color = {.r = u_char(color[0]), .g = u_char(color[1]), .b = u_char(color[2]), .a = u_char(color[3])},
			});
		
	for (auto &vertex : lines_buff) {
		vec4 currVecPos = vec4{vertex.Position.x, vertex.Position.y, vertex.Position.z, 1.0f};
		currVecPos = curr_xform * currVecPos;

		if (isFrustum) vertex.Position.y = -vertex.Position.y;

		if (currVecPos[3] != 0.0f) {
			vertex.Position.x = currVecPos[0] / currVecPos[3];
			vertex.Position.y = currVecPos[1] / currVecPos[3];
			vertex.Position.z = currVecPos[2] / currVecPos[3];
		}
		else {
			vertex.Position.x = currVecPos[0];
			vertex.Position.y = currVecPos[1];
			vertex.Position.z = currVecPos[2];
		}

	}
}


void Tutorial::traverse_root(S72::Node *root, std::vector< ObjectInstance > &scene_objects) {
    std::stack< NodeInfo > nodeStack;

    // get the root onto the stack

    mat4 root_parent_from_local = get_node_xform(root);

    NodeInfo rootNode {
        .node = root,
        .nodeTransform = root_parent_from_local,
    };

    nodeStack.push(rootNode);

	// keep track of which mesh instance each node controls by indexing into obj buffer (object_instances)
	//size_t idxInObjBuffer = 0;

    while(!nodeStack.empty()) {
        NodeInfo currNode = nodeStack.top();

        nodeStack.pop();

        if (currNode.node->mesh) {
            // add this mesh to an intermediate buffer, or get each property from mesh and then add those to separate buffers 
            // e.g. take the mesh and start processing the binary blob immediately using offset and count and then adding those to their own buffers per mesh property/xform/texture
            
            // emplace back into an objectInstance buffer 
            // 

            S72::Mesh *currMesh = currNode.node->mesh;
			//std::cout << "this is the current mesh: " << currNode.node->mesh->name << std::endl;
            std::string textureSource;
			std::string cubeTextureSource;
			std::string normalSource;
            uint8_t texType = 0;

            // std::cout << "meshct: " << currMesh->count << std::endl;
            // std::cout << "offset: " << currMesh->offset_in_static_buffer << std::endl;
			if (!currMesh->material) {
				//std::cout << "no material attached!" << std::endl;
				texType = 0;
				textureSource = "blank";
				//std::cout << "This is the texture source: " << textureSource << std::endl;
			}
			else {

				if (currMesh->material->normal_map) {
					//std::cout << "This material has a normal map! " << currMesh->name << currMesh->material->normal_map->src << std::endl;
					normalSource = currMesh->material->normal_map->src;
				}
				else {
					//std::cout << "This material does not have a normal map! " << currMesh->name << std::endl;
					normalSource = "defaultNorm";
				}
				
				//std::cout << "didnt crash 2: " << std::endl;

				if (S72::Material::Lambertian *brdfPtr = std::get_if<S72::Material::Lambertian> (&currMesh->material->brdf)) {
					texType = 0;
					
					if (S72::Texture **texturePtr = std::get_if<S72::Texture *> (&brdfPtr->albedo)) {
						
						textureSource = (*texturePtr)->src;
						//std::cout << "This is the texture source: " << textureSource << std::endl;
					}
					else if (std::holds_alternative<S72::color> (brdfPtr->albedo)) {
						//std::cout << "didnt crash 3" << std::endl;
						textureSource = currMesh->material->name;
						//std::cout << "didnt crash 4" << std::endl;
						//std::cout << "This is the texture source: " << textureSource << std::endl;
					}
					
				}

				else if (S72::Material::PBR *brdfPtr = std::get_if<S72::Material::PBR> (&currMesh->material->brdf)) {
					texType = 0;
					// albedo 
					if (S72::Texture **texturePtr = std::get_if<S72::Texture *> (&brdfPtr->albedo)) {
						textureSource = (*texturePtr)->src;
						//std::cout << "This is the texture source: " << textureSource << std::endl;
					}
					else if (std::holds_alternative<S72::color> (brdfPtr->albedo)) {
						textureSource = currMesh->material->name;
						//std::cout << "This is the texture source: " << textureSource << std::endl;
					}
					
					// roughness
					if (S72::Texture **texturePtr = std::get_if<S72::Texture *> (&brdfPtr->roughness)) {
						textureSource = (*texturePtr)->src;
						//std::cout << "This is the texture source: " << textureSource << std::endl;
					}
					else if (std::holds_alternative<float> (brdfPtr->roughness)) {
						textureSource = currMesh->material->name;
						//std::cout << "This is the texture source: " << textureSource << std::endl;
					}

					// metalness
					if (S72::Texture **texturePtr = std::get_if<S72::Texture *> (&brdfPtr->metalness)) {
						textureSource = (*texturePtr)->src;
						//std::cout << "This is the texture source: " << textureSource << std::endl;
					}
					else if (std::holds_alternative<float> (brdfPtr->metalness)) {
						textureSource = currMesh->material->name;
						//std::cout << "This is the texture source: " << textureSource << std::endl;
					}
				}

				else if (S72::Material::Environment *brdfPtr = std::get_if<S72::Material::Environment> (&currMesh->material->brdf)) {
					(void) brdfPtr;
					texType = 1;
					// under assumption only one environment
					//std::cout << "grabbing cube texture source" << std::endl;
					//std::cout << rtg.scene.environments.size() << std::endl;
				
					cubeTextureSource = rtg.scene.environments.begin()->second.name;
					//std::cout << "success! cube texture source: " << cubeTextureSource << std::endl;
				}

				else if (S72::Material::Mirror *brdfPtr = std::get_if<S72::Material::Mirror> (&currMesh->material->brdf)) {
					(void) brdfPtr;
					texType = 2;
					// under assumption only one environment
					//std::cout << "grabbing cube texture source" << std::endl;
					//std::cout << rtg.scene.environments.size() << std::endl;
					//cubeTextureSource = rtg.scene.environments[0].name;]
				
					cubeTextureSource = rtg.scene.environments.begin()->second.name;
					//std::cout << "success! cube texture source: " << cubeTextureSource << std::endl;
				}
			}

			// now this node knows where to index in object_instances
			currNode.node->obj_index = scene_objects.size();
			//std::cout << "this is the material name: " << textureSource << std::endl;
            ObjectInstance newObj{
				.objName = currMesh->name,
				.nodeName = currNode.node->name,
                .vertices {
                    .count = currMesh->count,
                    .first = static_cast<uint32_t>((currMesh->offset_in_static_buffer) / sizeof(PosNorTanTexVertex)),
                },
                .transform{
                        .CLIP_FROM_LOCAL = mat4{}, 
                        .WORLD_FROM_LOCAL = currNode.nodeTransform,
                        // take inverse bc not orthonormal
                        .WORLD_FROM_LOCAL_NORMAL = convert_back_to_mymat4(glm::inverse(convert_to_glm_mat4((currNode).nodeTransform))),
                 },
                .material = strings_to_material_idxs[currMesh->material->name],
				//.material = strings_to_material_idxs[textureSource],
				.cubeTexture = strings_to_cube_texture_idxs[cubeTextureSource],
				.texture_type = texType,
				//.texture = strings_to_texture_idxs["test"],
            };
            // std::cout << "This is the clip from world: \n" << std::endl;
            // print_matrix4x4(CLIP_FROM_WORLD * currNode.nodeTransform);
            // std::cout << "This is the texture: \n" << strings_to_texture_idxs[textureSource] <<  std::endl;
		

			std::vector< LinesPipeline::Vertex >currbbox_lines_buffer;
			currbbox_lines_buffer.clear();
			draw_bbox(currbbox_lines_buffer, currMesh->mesh_min, currMesh->mesh_max, currNode.nodeTransform, false);
			newObj.bbox_lines = currbbox_lines_buffer;

			scene_objects.emplace_back(newObj);
			// idxInObjBuffer++;
			//std::cout << "size after " << lines_vertices.size() << std::endl;
        } 

		if (currNode.node->camera) {
			vec4 newCameraPos = {0, 0, 0, 1};

			// vec3 camPos = currNode.node->translation;
			newCameraPos = currNode.nodeTransform * newCameraPos;
			

			auto camPerspective = std::get< S72::Camera::Perspective > (currNode.node->camera->projection);

			Camera newSceneCam {
				.position = newCameraPos,
				.transform = currNode.nodeTransform,

				.aspect = camPerspective.aspect,
				.fov = camPerspective.vfov,
				.near = camPerspective.near,
				.far = camPerspective.far,
			};

			// change this so that i can track the names of the cameras as well
			scene_cameras[currNode.node->camera->name] = newSceneCam;

			//std::cout << "finished cameras" << std::endl;
		}

		if (currNode.node->light) {
			//std::cout << "This is a light" << std::endl;
			vec4 light = {0, 0, 0, 1};
			vec4 light_dir = {0, 0, 1, 0};

			light = currNode.nodeTransform * light;
			light_dir = currNode.nodeTransform * light_dir;

			// std::cout << "This is the current light: " << currNode.node->name << std::endl;
			// print_matrix4x4(currNode.nodeTransform);
			// std::cout << "resulting light position" << std::endl;
			// print_vector_4x1(light);
			// std::cout << "resulting light direction" << std::endl;
			// print_vector_4x1(light_dir);


			if (S72::Light::Sun *sunPtr = std::get_if<S72::Light::Sun>(&currNode.node->light->source)) {
				//std::cout << "sun angle " << sunPtr->angle << std::endl;

				sunlight_insts.emplace_back(ObjectsPipeline::Sun_Light{
					.angle_w_pad{
						.angle = (sunPtr->angle / 2.0f),
					},
					.SUN_DIRECTION{
						.x = light[0],
						.y = light[1],
						.z = light[2],
					},
					.SUN_ENERGY{
						.r = currNode.node->light->tint.r * sunPtr->strength,
					    .g = currNode.node->light->tint.g * sunPtr->strength,
				        .b = currNode.node->light->tint.b * sunPtr->strength,
					}
				});
			}

			else if (S72::Light::Sphere *spherePtr = std::get_if<S72::Light::Sphere>(&currNode.node->light->source)) {
				
				spherelight_insts.emplace_back(ObjectsPipeline::Sphere_Light{ 
					.SPHERE_POSITION_AND_RADIUS{
						.x = light[0],
						.y = light[1],
						.z = light[2],
						.radius = spherePtr->radius,
					},
					.SPHERE_POWER_AND_LIMIT{
						.r = currNode.node->light->tint.r * spherePtr->power,
						.g = currNode.node->light->tint.g * spherePtr->power,
						.b = currNode.node->light->tint.b * spherePtr->power,
						.limit = spherePtr->limit,
					}
					
				});
				//std::cout << "This is the sphere light's position: " << spherelight_insts[0].SPHERE_POSITION_AND_RADIUS.x << "," <<spherelight_insts[0].SPHERE_POSITION_AND_RADIUS.y << "," << spherelight_insts[0].SPHERE_POSITION_AND_RADIUS.z << std::endl;
			}

			else if (S72::Light::Spot *spotPtr = std::get_if<S72::Light::Spot>(&currNode.node->light->source)) {
				spotlight_insts.emplace_back(ObjectsPipeline::Spot_Light{
					.SPOT_POSITION_AND_RADIUS{
						.x = light[0],
						.y = light[1],
						.z = light[2],
						.radius = spotPtr->radius,
					},
					.SPOT_DIRECTION{
						.x = light_dir[0],
						.y = light_dir[1],
						.z = light_dir[2],
					},
					.SPOT_POWER_AND_LIMIT{
						.r = currNode.node->light->tint.r * spotPtr->power,
						.g = currNode.node->light->tint.g * spotPtr->power,
						.b = currNode.node->light->tint.b * spotPtr->power,
						.limit = spotPtr->limit,
					},
					.SPOT_OUTER_AND_INNER_LIM{
						.outer = spotPtr->fov / 2.0f,
						.inner = (spotPtr->fov * (1 - spotPtr->blend)) / 2.0f,
					}
				});
				//std::cout << "This is the spot light's direction: " << spotlight_insts[0].SPOT_DIRECTION.x << "," <<spotlight_insts[0].SPOT_DIRECTION.y << "," << spotlight_insts[0].SPOT_DIRECTION.z << std::endl;
			}
		}

		if (currNode.node -> environment) {

		}

        for (S72::Node *child : currNode.node->children) {
            NodeInfo childNode {
                .node = child,
                .nodeTransform = currNode.nodeTransform * get_node_xform(child),
            };
            
            nodeStack.push(childNode);
        }
    }
}


void Tutorial::on_input(InputEvent const &evt) {
	// maybe add a button press to add shapes to the scene?

	// if there is a current action, it gets input priority 
	if (action) {
		action(evt);
		return;
	}

	// change exposure 
	if (evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_RIGHT_BRACKET) {
		rtg.exposure += 0.2;
		return;
	}

	if (evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_LEFT_BRACKET) {
		rtg.exposure -= 0.2;
		return;
	}
	
	// general controls
	if (evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_1) {
		// switch camera modes
		// FOR SOME REASON, HOLDING DOWN key1 MAKES THE SCREEN BLACK - TODO FIX THIS
		if (camera_mode != CameraMode::Scene){
			prevCamera = currCamera;
			camera_mode = CameraMode(0);
		}
		//currCamera = scene_camera;
		
		//scene_camera_index = 0;

		//update_camera(currCamera);

		return;
	}

	if (evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_2) {
		// switch camera modes
		if (camera_mode != CameraMode(1)){
			prevCamera = currCamera;
			camera_mode = CameraMode(1);
		}
		//currCamera = free_camera;
		//camera_mode = CameraMode(1);
		//scene_camera_index = 0;
		return;
	}

	if (evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_0) {
		// switch camera modes
		if (camera_mode != CameraMode(2)){
			prevCamera = currCamera;
			camera_mode = CameraMode(2);
		}
		//currCamera = debug_camera;
		//camera_mode = CameraMode(2);
		//scene_camera_index = 0;
		return;
	}

	if (camera_mode == CameraMode::Scene) {
		// use arrow keys to swap between scene cameras
		// currCamera = scene_cameras[map_camera_idxs_to_string[scene_camera_index]];
		// camera_aspect = (currCamera).aspect;

		if (evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_RIGHT) {
			scene_camera_index = (scene_camera_index + 1) % total_cameras;
			
			// SceneCamera currCamera = scene_cameras[scene_camera_index];
			return;
		}

		if (evt.type == InputEvent::KeyDown && evt.key.key == GLFW_KEY_LEFT) {
			scene_camera_index = (scene_camera_index - 1) % -total_cameras;
			//std::cout << scene_camera_index << std::endl;
			return;
		}
		//return;
	}

	// free camera controls
	if (camera_mode == CameraMode::Free) {
		//currCamera = free_camera;
		camera_aspect = free_camera.aspect;

		// dollying 
		if (evt.type == InputEvent::MouseWheel) {
			// change distance by 10% every scroll click:
			free_camera.radius *= std::exp(std::log(1.1f) * -evt.wheel.y);
			// make sure camera isn't too close or too far from target:
			free_camera.radius = std::max(free_camera.radius, 0.5f * free_camera.near);
			free_camera.radius = std::min(free_camera.radius, 2.0f * free_camera.far);
			return;
		}

		// panning
		if (evt.type == InputEvent::MouseButtonDown && evt.button.button == GLFW_MOUSE_BUTTON_LEFT && (evt.button.mods & GLFW_MOD_SHIFT)) {
			// start panning
			float init_x = evt.button.x;
			float init_y = evt.button.y;
			Camera init_camera = free_camera;

			action = [this, init_x, init_y, init_camera](InputEvent const &evt) {
				if (evt.type == InputEvent::MouseButtonUp && evt.button.button == GLFW_MOUSE_BUTTON_LEFT) {
					// cancel upon button lifted:
					action = nullptr;
					return;
				}
				if (evt.type == InputEvent::MouseMotion) {
					// image height at plane of target point:
					float height = 2.0f * std::tan(free_camera.fov * 0.5f) * free_camera.radius;
					
					// motion, therefore, at target point: (div converts pixels to distances at image plane)
					float dx = (evt.motion.x - init_x) / rtg.swapchain_extent.height * height;
					float dy = -(evt.motion.y - init_y) / rtg.swapchain_extent.height * height;

					// compute camera transform to extract right (first row) and up (second row):
					mat4 camera_from_world = orbit(init_camera.target, 
												   init_camera.azimuth, 
												   init_camera.elevation, 
												   init_camera.radius);

					// move the desired distance:
					free_camera.target[0] = init_camera.target[0] - dx * camera_from_world[0] - dy * camera_from_world[1];
					free_camera.target[1] = init_camera.target[1] - dx * camera_from_world[4] - dy * camera_from_world[5];
					free_camera.target[2] = init_camera.target[2] - dx * camera_from_world[8] - dy * camera_from_world[9];
					
					return;
				}
			};

			return;
		}

		// tumbling
		if (evt.type == InputEvent::MouseButtonDown && evt.button.button == GLFW_MOUSE_BUTTON_LEFT) {
			// start tumbling

			float init_x = evt.button.x;
			float init_y = evt.button.y;
			Camera init_camera = free_camera;

			// [this] caputres a copy of -this- pointer
			action = [this, init_x, init_y, init_camera](InputEvent const &evt) {
				if (evt.type == InputEvent::MouseButtonUp && evt.button.button == GLFW_MOUSE_BUTTON_LEFT) {
					// cancel upon button lifted:
					action = nullptr;
					return;
				}

				if (evt.type == InputEvent::MouseMotion) {
					// motion, normalized so 1.0 is window height
					float dx = (evt.motion.x - init_x) / rtg.swapchain_extent.height;
					float dy = -(evt.motion.y - init_y) / rtg.swapchain_extent.height; // negated bc glfw uses y-down coord system
					
					// rotate camera based on motion:
					float speed = float(M_PI); // how much rotation happens at one full window height
					// compensates for when camera is upside down					
					float flip_x = (std::abs(init_camera.elevation) > 0.5f * float(M_PI) ? -1.0f : 1.0f);
					free_camera.azimuth = init_camera.azimuth - dx * speed * flip_x;
					free_camera.elevation = init_camera.elevation - dy * speed;

					//reduce azimuth and elevation to [-pi, pi] range: maybe change to [-pi, pi) range later on
					const float twopi = 2.0f * float(M_PI);
					free_camera.azimuth -= std::round(free_camera.azimuth / twopi) * twopi;
					free_camera.elevation -= std::round(free_camera.elevation / twopi) * twopi;
					return;
				}
			};

			return;
		}
	}

	// free camera controls
	if (camera_mode == CameraMode::Debug) {
		//currCamera = debug_camera;
		camera_aspect = debug_camera.aspect;

		// dollying 
		if (evt.type == InputEvent::MouseWheel) {
			// change distance by 10% every scroll click:
			debug_camera.radius *= std::exp(std::log(1.1f) * -evt.wheel.y);
			// make sure camera isn't too close or too far from target:
			debug_camera.radius = std::max(debug_camera.radius, 0.5f * debug_camera.near);
			debug_camera.radius = std::min(debug_camera.radius, 2.0f * debug_camera.far);
			return;
		}

		// panning
		if (evt.type == InputEvent::MouseButtonDown && evt.button.button == GLFW_MOUSE_BUTTON_LEFT && (evt.button.mods & GLFW_MOD_SHIFT)) {
			// start panning
			float init_x = evt.button.x;
			float init_y = evt.button.y;
			Camera init_camera = debug_camera;

			action = [this, init_x, init_y, init_camera](InputEvent const &evt) {
				if (evt.type == InputEvent::MouseButtonUp && evt.button.button == GLFW_MOUSE_BUTTON_LEFT) {
					// cancel upon button lifted:
					action = nullptr;
					return;
				}

				if (evt.type == InputEvent::MouseMotion) {
					// image height at plane of target point:
					float height = 2.0f * std::tan(debug_camera.fov * 0.5f) * debug_camera.radius;
					
					// motion, therefore, at target point: (div converts pixels to distances at image plane)
					float dx = (evt.motion.x - init_x) / rtg.swapchain_extent.height * height;
					float dy = -(evt.motion.y - init_y) / rtg.swapchain_extent.height * height;

					// compute camera transform to extract right (first row) and up (second row):
					mat4 camera_from_world = orbit(init_camera.target, 
												   init_camera.azimuth, 
												   init_camera.elevation, 
												   init_camera.radius);

					// move the desired distance:
					debug_camera.target[0] = init_camera.target[0] - dx * camera_from_world[0] - dy * camera_from_world[1];
					debug_camera.target[1] = init_camera.target[1] - dx * camera_from_world[4] - dy * camera_from_world[5];
					debug_camera.target[2] = init_camera.target[2] - dx * camera_from_world[8] - dy * camera_from_world[9];
					
					return;
				}
			};

			return;
		}

		// tumbling
		if (evt.type == InputEvent::MouseButtonDown && evt.button.button == GLFW_MOUSE_BUTTON_LEFT) {
			// start tumbling

			float init_x = evt.button.x;
			float init_y = evt.button.y;
			Camera init_camera = debug_camera;

			// [this] caputres a copy of -this- pointer
			action = [this, init_x, init_y, init_camera](InputEvent const &evt) {
				if (evt.type == InputEvent::MouseButtonUp && evt.button.button == GLFW_MOUSE_BUTTON_LEFT) {
					// cancel upon button lifted:
					action = nullptr;
					return;
				}

				if (evt.type == InputEvent::MouseMotion) {
					// motion, normalized so 1.0 is window height
					float dx = (evt.motion.x - init_x) / rtg.swapchain_extent.height;
					float dy = -(evt.motion.y - init_y) / rtg.swapchain_extent.height; // negated bc glfw uses y-down coord system
					
					// rotate camera based on motion:
					float speed = float(M_PI); // how much rotation happens at one full window height
					// compensates for when camera is upside down					
					float flip_x = (std::abs(init_camera.elevation) > 0.5f * float(M_PI) ? -1.0f : 1.0f);
					debug_camera.azimuth = init_camera.azimuth - dx * speed * flip_x;
					debug_camera.elevation = init_camera.elevation - dy * speed;

					//reduce azimuth and elevation to [-pi, pi] range: maybe change to [-pi, pi) range later on
					const float twopi = 2.0f * float(M_PI);
					debug_camera.azimuth -= std::round(debug_camera.azimuth / twopi) * twopi;
					debug_camera.elevation -= std::round(debug_camera.elevation / twopi) * twopi;
					return;
				}
			};

			return;
		}
	}

}


void Tutorial::update_camera(Camera &camera) {

	mat4 persp = perspective(
					(camera).fov,
					(camera).aspect,
					(camera).near,
					(camera).far
				);

	camera_aspect = (camera).aspect;
	
	CLIP_FROM_WORLD = convert_back_to_mymat4(convert_to_glm_mat4(persp) * glm::inverse(convert_to_glm_mat4((camera).transform)));
	camera.currClipFromWorld = CLIP_FROM_WORLD;
}



// CODE GRAVEYARD:



// { // load in environments 
	// 	std::cout << "Loading in environments!" << std::endl;
	// 	for (auto enviro : rtg.scene.environments) {
	// 		int tex_img_width, tex_img_height, channels;
	// 		stbi_uc *img = stbi_load(enviro.second.radiance->path.c_str(), &tex_img_width, &tex_img_height, &channels, 4);
	// 		std::cout << "tex_image: " << tex_img_width <<"," << tex_img_height << std::endl;
	// 		uint32_t image_byte_size = 4 * tex_img_width * tex_img_height;

	// 		std::vector< uint32_t > data;
	// 		data.reserve(tex_img_width * tex_img_height);
	// 		// https://registry.khronos.org/OpenGL/specs/gl/glspec30.pdf page 181-182
	// 		for (uint32_t i = 0; i < image_byte_size; i += 4) {
	// 			stbi_uc R = *(img + i);
	// 			stbi_uc G = *(img + i + 1);
	// 			stbi_uc B = *(img + i + 2);
	// 			stbi_uc E = *(img + i + 3);

	// 			// float R_prime, G_prime, B_prime;
	// 			uint32_t rgbe_res;

	// 			if (R == 0 && G == 0 && B == 0 && E == 0) {
	// 				rgbe_res = 0;
	// 			}

	// 			else {
	// 				rgb9_e5_from_rgbe8(R, G, B, E, &rgbe_res);
	// 			}

	// 			data[i / 4] = rgbe_res;
	// 		}
			
	// 		cube_textures.emplace_back(rtg.helpers.create_image(
	// 			VkExtent2D{ .width = (uint) tex_img_width, .height = (uint) tex_img_height}, // size of image
	// 			VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, // how to interpret image data 
	// 			VK_IMAGE_TILING_OPTIMAL,
	// 			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // will sample and upload
	// 			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // should be device_local
	// 			Helpers::Unmapped
	// 		));
	// 		rtg.helpers.transfer_to_cube_image(data.data(), image_byte_size, cube_textures.back());
	// 		stbi_image_free(img);

	// 		// temporary because only 1 enviornment 
	// 		strings_to_cube_texture_idxs.insert({enviro.second.name, cube_textures.size() - 1});
	// 	}
	// 	std::cout << "Loaded in environments" << std::endl;

	// }