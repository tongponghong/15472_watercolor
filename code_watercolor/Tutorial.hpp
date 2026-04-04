#pragma once

#include "VertexTypes/PosColVertex.hpp"
#include "VertexTypes/PosNorTexVertex.hpp"
#include "VertexTypes/PosNorTanTexVertex.hpp"
#include "VertexTypes/PosNorVertex.hpp"
#include "mathlibs/mat4.hpp"
#include "RTG.hpp"
#include "helperlibs/sceneGrapher/sceneGrapher.hpp"

#include "helperlibs/stb_image_lib/stb_image.h"

struct Tutorial : RTG::Application {

	Tutorial(RTG &);
	Tutorial(Tutorial const &) = delete; //you shouldn't be copying this object
	~Tutorial();

	//kept for use in destructor:
	RTG &rtg;

	//--------------------------------------------------------------------
	//Resources that last the lifetime of the application:

	//chosen format for depth buffer:
	VkFormat depth_format{};
	//Render passes describe how pipelines write to images:
	VkRenderPass render_pass = VK_NULL_HANDLE;
	VkRenderPass shadow_render_pass = VK_NULL_HANDLE;

	//Pipelines:

	struct BackgroundPipeline {
		// no descriptor set layouts

		// push constants
		struct Push {
			float time;
		};

		VkPipelineLayout layout = VK_NULL_HANDLE;

		// no vertex bindings
		
		VkPipeline handle = VK_NULL_HANDLE;

		void create (RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy (RTG &);
	} background_pipeline;

	struct LinesPipeline {
		// descriptor set layouts
		// a descriptor is pointer to resource in GPU memory with a specific type
		// for how the shader will use the memory chunk it points to
		VkDescriptorSetLayout set0_Camera = VK_NULL_HANDLE;

		// types for descriptors 
		struct Camera {
			mat4 CLIP_FROM_WORLD;
		};
		static_assert(sizeof(Camera) == 16 * 4, "camera buffer struct is packed");

		// no push constants

		VkPipelineLayout layout = VK_NULL_HANDLE;

		// vertex bindings
		
		using Vertex = PosColVertex;

		VkPipeline handle = VK_NULL_HANDLE;

		void create (RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy (RTG &);
	} lines_pipeline;

	struct ObjectsPipeline {
		// descriptor set layouts
		// a descriptor is pointer to resource in GPU memory with a specific type
		// for how the shader will use the memory chunk it points to

		VkDescriptorSetLayout set0_SUN = VK_NULL_HANDLE;
		
		VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;

		VkDescriptorSetLayout set2_SPHERE = VK_NULL_HANDLE;
		VkDescriptorSetLayout set3_SPOT = VK_NULL_HANDLE;
		VkDescriptorSetLayout set4_TEXTURE = VK_NULL_HANDLE;
		// VkDescriptorSetLayout set2_TEXTURE_CUBE = VK_NULL_HANDLE;
		// VkDescriptorSetLayout set2_TEXTURE_LAMB = VK_NULL_HANDLE;

		// types for descriptors 
		struct Sun_Light {
			// struct { float x, y, z, padding_; } SKY_DIRECTION;
			// struct { float r, g, b, padding_; } SKY_ENERGY;
			struct { float x, y, z, padding_; } SUN_DIRECTION;
		
			struct { float r, g, b, padding_; } SUN_ENERGY;
			struct { float angle, pad1, pad2, pad3; } angle_w_pad;

		};

		static_assert(sizeof(Sun_Light) == 4 * 4 + 4 * 4 + 4 * 4, "sunlight is the expected size.");
		
		std::vector< Sun_Light > sun_lights;

		struct Sphere_Light {
			struct { float x, y, z, radius; } SPHERE_POSITION_AND_RADIUS;
			struct { float r, g, b, limit; } SPHERE_POWER_AND_LIMIT;
		};
		static_assert(sizeof(Sphere_Light) == 4 * 4 + 4 * 4, "sphere is the expected size.");

		struct Spot_Light {
			struct { float x, y, z, radius; } SPOT_POSITION_AND_RADIUS;
			struct { float x, y, z, padding_; } SPOT_DIRECTION;
			struct { float r, g, b, limit; } SPOT_POWER_AND_LIMIT;
			struct { float outer, inner, pad1, pad2; } SPOT_OUTER_AND_INNER_LIM;
		};
		static_assert(sizeof(Spot_Light) == 4 * 4 + 4 * 4 + 4 * 4 + 4 * 4, "spot is the expected size.");

		struct Transform {
			mat4 CLIP_FROM_LOCAL;
			mat4 WORLD_FROM_LOCAL;
			mat4 WORLD_FROM_LOCAL_NORMAL;
		};
		static_assert(sizeof(Transform) == 16 * 4 + 16 * 4 + 16 * 4, "Transform is the expected size.");


		// push constants
		struct Push {
			vec3 EYE_vec; // to get the incident camera vector 
			int tex_type; // 1 is env, 2 is mirror, 0 is any else
			float exposure;
			float time;
		};

		struct spec_consts {
            int num_sun, num_sphere, num_spot, tonemap_type;
        };

		VkPipelineLayout layout = VK_NULL_HANDLE;

		// vertex bindings
		
		using Vertex = PosNorTanTexVertex;

		VkPipeline handle = VK_NULL_HANDLE;

		void create (RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy (RTG &);
	} objects_pipeline;

	struct ShadowsPipeline {
		VkDescriptorSetLayout set0_SHADOWS;

		// push constants
		struct Push {
			mat4 LIGHT_FROM_WORLD;
		};

		VkPipelineLayout layout = VK_NULL_HANDLE;

		VkPipeline handle = VK_NULL_HANDLE;

		using Vertex = PosNorTanTexVertex;

		void create (RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy (RTG &);
	} shadows_pipeline;

	//pools from which per-workspace things are allocated:
	VkCommandPool command_pool = VK_NULL_HANDLE;
	VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
	
	//STEPX: Add descriptor pool here.


	//workspaces hold per-render resources:
	struct Workspace {
		VkCommandBuffer command_buffer = VK_NULL_HANDLE; //from the command pool above; reset at the start of every render.

		// location for lines data: (streamed to GPU per-frame) 
		Helpers::AllocatedBuffer lines_vertices_src; // host coherent; mapped
		Helpers::AllocatedBuffer lines_vertices; // device-local

		// location for LinesPipeline::Camera data: (streamed to GPU per-frame)
		Helpers::AllocatedBuffer Camera_src; // host coherent; mapped
		Helpers::AllocatedBuffer Camera; // device-local
		VkDescriptorSet Camera_descriptors; // references camera 

		// // location for ObjectsPipeline::Transforms data: (streamed to GPU per-frame)
		// Helpers::AllocatedBuffer Transforms_src; // host coherent; mapped
		// Helpers::AllocatedBuffer Transforms; // device-local
		// VkDescriptorSet Transforms_descriptors; // references Transforms

		// location for ObjectsPipeline::Transforms data: (streamed to GPU per-frame)
		Helpers::AllocatedBuffer Transforms_src; // host coherent; mapped
		Helpers::AllocatedBuffer Transforms; // device-local
		VkDescriptorSet Transforms_descriptors; // references Transforms

		Helpers::AllocatedBuffer Sunlights_src; // host coherent; mapped
		Helpers::AllocatedBuffer Sunlights; // device-local
		VkDescriptorSet Sunlights_descriptors; // references Transforms

		Helpers::AllocatedBuffer Spherelights_src; // host coherent; mapped
		Helpers::AllocatedBuffer Spherelights; // device-local
		VkDescriptorSet Spherelights_descriptors; // references Transforms

		Helpers::AllocatedBuffer Spotlights_src; // host coherent; mapped
		Helpers::AllocatedBuffer Spotlights; // device-local
		VkDescriptorSet Spotlights_descriptors; // references Transforms
	};
	std::vector< Workspace > workspaces;

	//-------------------------------------------------------------------
	//static scene resources:

	Helpers::AllocatedBuffer object_vertices;
	struct ObjectVertices { // can be used in dictionaries to store MANY objects!
		uint32_t first = 0; // index of first vertex
		uint32_t count = 0; // num of vertices 
	}; 
	ObjectVertices plane_vertices;
	ObjectVertices torus_vertices;
	ObjectVertices icosphere_vertices;

	// moved for convenience (even tho both static and not)
	struct Obj_Material {
		uint32_t texture_idx;
		uint32_t normal_map_tex_idx;
		uint32_t roughness_map_tex_idx;
		uint32_t metalness_map_tex_idx;
	};

	struct ObjectInstance {
		std::string objName; // name of the mesh 
		std::string nodeName; // node that references the mesh
		ObjectVertices vertices;
		std::vector< LinesPipeline::Vertex > bbox_lines;

		ObjectsPipeline::Transform transform;
		uint32_t material = 0;
		uint32_t cubeTexture = 0;
		uint32_t normalMap = 0;
		uint8_t texture_type = 0; // 1 is env, 2 is mirror, 0 is any else
	};
	std::vector< ObjectInstance > object_instances;
	std::vector< ObjectInstance > user_object_instances;

	std::vector< Obj_Material > obj_materials;
	std::vector< Helpers::AllocatedImage > textures_in_use;
	std::vector< Helpers::AllocatedImage > cube_textures;
	std::vector< Helpers::AllocatedImage > lambertian_lookup_tex;

	// map a texture name to the index it shows up in the texture descriptors 
	std::unordered_map< std::string, uint32_t > strings_to_texture_idxs;
	std::unordered_map< std::string, uint32_t > strings_to_cube_texture_idxs;
	std::unordered_map< std::string, uint32_t > strings_to_material_idxs;
	
	// std::vector< VkImageView > texture_views;
	// VkSampler texture_sampler = VK_NULL_HANDLE;
	VkDescriptorPool texture_descriptor_pool = VK_NULL_HANDLE;
	// std::vector< VkDescriptorSet > texture_descriptors; // allocated from texture_descriptor_pool

	std::vector< VkImageView > material_views;
	VkSampler material_sampler = VK_NULL_HANDLE;
	// VkDescriptorPool material_descriptor_pool = VK_NULL_HANDLE;
	std::vector< VkDescriptorSet > material_descriptors; // allocated from texture_descriptor_pool

	// cubes 
	std::vector< VkImageView > cube_texture_views;
	VkSampler cube_texture_sampler = VK_NULL_HANDLE;
	VkDescriptorPool cube_texture_descriptor_pool = VK_NULL_HANDLE;
	std::vector< VkDescriptorSet > cube_texture_descriptors; // allocated from cube_texture_descriptor_pool

	std::vector< VkImageView > lambertian_lookup_views;
	std::vector< VkDescriptorSet > lamb_texture_descriptors; // allocated from cube_texture_descriptor_pool


	std::vector< ObjectsPipeline::Sun_Light > sunlight_insts; 
	std::vector< ObjectsPipeline::Sphere_Light > spherelight_insts; 
	std::vector< ObjectsPipeline::Spot_Light > spotlight_insts; 

	// SCENE GRAPH 
	struct NodeInfo{
		S72::Node *node;
		mat4       nodeTransform;
	};

	void traverse_scene(S72 &scene, std::vector< ObjectInstance > &scene_objects);

	mat4 get_node_xform(S72::Node *node);

	void traverse_root(S72::Node *root, std::vector< ObjectInstance > &scene_objects);

	void draw_bbox(std::vector< LinesPipeline::Vertex > &lines_buff, vec3 box_min, vec3 box_max, mat4 curr_xform, bool isFrustum);

	//--------------------------------------------------------------------
	//Resources that change when the swapchain is resized:

	virtual void on_swapchain(RTG &, RTG::SwapchainEvent const &) override;

	Helpers::AllocatedImage swapchain_depth_image;
	VkImageView swapchain_depth_image_view = VK_NULL_HANDLE;
	std::vector< VkFramebuffer > swapchain_framebuffers;
	//used from on_swapchain and the destructor: (framebuffers are created in on_swapchain)
	void destroy_framebuffers();

	//--------------------------------------------------------------------
	// Shadow pass resources

	// change this later to be optimized
	VkExtent2D shadowAtlasExtent {
		.height = 4096,
		.width = 4096,
	};

	Helpers::AllocatedImage shadow_atlas_image;
	VkImageView shadow_atlas_image_view = VK_NULL_HANDLE;
	VkFramebuffer shadowmap_framebuffer;

	//--------------------------------------------------------------------
	//Resources that change when time passes or the user interacts:

	virtual void update(float dt) override;
	virtual void on_input(InputEvent const &) override;
	
	// model action, intercepts inputs:
	std::function< void(InputEvent const &) > action;

	float time = 0.0f;

	// for selecting between cameras
	enum class CameraMode {
		Scene = 0,
		Free = 1,
		Debug = 2,
	} camera_mode;

	// used when camera_mode == CameraMode::Free or debug
	struct Camera {
		vec3 target = vec3{0, 0, 0}; // where the camera is looking + orbiting
		float radius = 2.0f; // distance from camera to target
		float azimuth = 0.0f; // CCW angle around z axis between x axis and camera directon (radians)
		float elevation = 0.25f * float(M_PI); // angle up from xy plane to camera direction (radians)

		// used by scene cameras 
		vec4 position;
		mat4 transform;

		mat4 currClipFromWorld = identity_4x4();

		float fov = 60.0f / 180.0f * float(M_PI); // vertical field of view (radians)
		float near = 0.1f; // near clipping plane
		float aspect = 16.0f / 9.0f;
		float far = 1000.0f; // far clipping plane
	} free_camera, debug_camera, scene_camera;

	Camera currCamera;
	Camera prevCamera;

	void update_camera(Camera &camera);

	// struct SceneCamera {
		

	// 	float fov = 60.0f / 180.0f * float(M_PI); // vertical field of view (radians)
	// 	float near = 0.1f; // near clipping plane
	// 	float aspect = 0.0f;
	// 	float far = 1000.0f; // far clipping plane
	// } scene_camera;

	// std::variant<SceneCamera, OrbitCamera> currCamera;
	// std::variant<SceneCamera, OrbitCamera> prevCamera;

	size_t total_cameras;
	int32_t scene_camera_index = 0;

	std::map< std::string, Camera > scene_cameras;
	std::map< int32_t, std::string> map_camera_idxs_to_string;
	float camera_aspect = 16.0f / 9.0f;
	
	// computed from the current camera (as set by camera_mode) during update() 
	mat4 CLIP_FROM_WORLD = identity_4x4();


	std::vector< LinesPipeline::Vertex > lines_vertices;
	
	// ObjectsPipeline::Sun_Light sunlight;
	// ObjectsPipeline::Sphere_Light spherelight;
	// ObjectsPipeline::Spot_Light spotlight;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
