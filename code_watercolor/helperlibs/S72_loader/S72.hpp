//#include "../sejp_lib/sejp.hpp"

#include "sejp_lib/sejp.hpp"
#include "mathlibs/mat4.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.h>

#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#pragma once

/*
 * Represents a scene in s72 format.
 * Specification: https://github.com/15-472/s72
 *
 * - Resolves name-based references to pointers.
 * - Resolves file paths relative to scene file.
 * - DOES NOT attempt to load texture or data files.
 * - Stores objects in name-based dictionaries.
 * - Otherwise, represents scene just as in the spec.
 *
 * Usage:
 *  try {
 *    S72 scene = S72::load("path/to/my_s72_scene.s72");
 *    //scene loading succeeded!
 *  } catch (std::exception &e) {
 *    std::cerr << "Scene loading failed:\n" << e.what() << std::endl;
 *  }
 *
 */

struct S72 {
	//NOTE: redefine these for your vector and quaternion types of choice:
	// using vec3 = struct vec3_internal{ float x, y, z; };
	using vec3 = std::array < float, 3 >;
	// using quat = struct quat_internal{ float x, y, z, w; };
	using quat = vec4;
	using color = struct color_internal{ float r, g, b; };

	//-------------------------------------------------

	static S72 load(std::string const &file); //NOTE: throws on error

	S72() = default; //empty scene

	//no copy constructor because it would require pointe fixups:
	S72(S72 const &) = delete;
	S72 &operator=(S72 const &) = delete;

	//move constructor okay:
	S72(S72 &&) = default;
	S72 &operator=(S72 &&) = default;

	//forward declarations so we can write the scene's objects in the same order as in the spec:
	struct Node;
	struct Mesh;
	struct DataFile;
	struct Camera;
	struct Driver;
	struct Texture;
	struct Material;
	struct Environment;
	struct Light;

	//-------------------------------------------------
	//s72 Scenes contain:

	//exactly one scene:
	struct Scene {
		std::string name;
		std::vector< Node * > roots;
	};
	Scene scene;

	//zero or more "NODE"s, all with unique names:
	struct Node {
		std::string name;

		// vec3 translation = vec3{ .x = 0.0f, .y = 0.0f, .z = 0.0f };
		vec3 translation = vec3{0.0f, 0.0f, 0.0f};
		// quat rotation = quat{ .x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f };
		quat rotation = quat{0.0f, 0.0f, 0.0f, 1.0f};
		// vec3 scale = vec3{ .x = 1.0f, .y = 1.0f, .z = 1.0f };
		vec3 scale = vec3{1.0f, 1.0f, 1.0f};

		std::vector< Node * > children;

		//optional, null if not specified:
		size_t obj_index;
		Mesh *mesh = nullptr;
		Camera *camera = nullptr;
		Environment *environment = nullptr;
		Light *light = nullptr;
	};
	std::unordered_map< std::string, Node > nodes;

	//zero or more "MESH"s, all with unique names:
	struct Mesh {
		std::string name;
		uint32_t offset_in_static_buffer = 0;
		vec3 mesh_min = vec3{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
		vec3 mesh_max = vec3{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};

		VkPrimitiveTopology topology;
		uint32_t count;
		struct Indices {
			DataFile &src;
			uint32_t offset;
			VkIndexType format;
		};
		std::optional< Indices > indices; //mesh index stream; optional

		struct Attribute {
			DataFile &src;
			uint32_t offset;
			uint32_t stride;
			VkFormat format;
		};
		std::unordered_map< std::string, Attribute > attributes;

		Material *material = nullptr; //optional, null if not specified
	};
	std::unordered_map< std::string, Mesh > meshes;

	//data files referenced by meshes:
	struct DataFile {
		std::string src; //src used in the s72 file

		//computed during loading:
		std::string path; //path to data file, taking into account path to s72 file (relative to current working directory)
	};
	//we organize the data files by "src" so that multiple attributes with the same src resolve to the same DataFile:
	std::unordered_map< std::string, DataFile > data_files;

	//zero or more "CAMERA"s, all with unique names:
	struct Camera {
		std::string name;

		struct Perspective {
			float aspect;
			float vfov;
			float near;
			float far = std::numeric_limits< float >::infinity(); //optional, if not specified will be set to infinity
		};

		//(s72 leaves open the possibility of other camera projections, but does not define any)

		std::variant< Perspective > projection;
	};
	std::unordered_map< std::string, Camera > cameras;

	//zero or more "DRIVER"s, all with unique names:
	struct Driver {
		std::string name;

		Node &node;

		enum class Channel {
			translation,
			scale,
			rotation
		} channel;

		std::vector< float > times;
		std::vector< float > values;
		uint32_t keyFrameNum = 0;

		enum class Interpolation {
			STEP,
			LINEAR,
			SLERP,
		} interpolation = Interpolation::LINEAR;
	};
	//NOTE: drivers are stored in a vector in the order they appear in the file.
	//      This is because drivers are applied in file order:
	std::vector< Driver > drivers;

	//zero or more "MATERIAL"s, all with unique names:
	struct Material {
		std::string name;

		Texture *normal_map = nullptr; //optional, set to null if not specified
		Texture *displacement_map = nullptr; //optional, set to null if not specified

		//Materials are one of these types:
		// NOTE: if any of these parameters are the Texture * branch of their variant, they are not null
		struct PBR {
			std::variant< color, Texture * > albedo = color{.r = 1.0f, .g = 1.0f, .b = 1.0f};
			std::variant< float, Texture * > roughness = 1.0f;
			std::variant< float, Texture * > metalness = 0.0f;
		};
		struct Lambertian {
			std::variant< color, Texture * > albedo = color{.r = 1.0f, .g = 1.0f, .b = 1.0f};
		};
		struct Mirror { /* no parameters */ };
		struct Environment { /* no parameters */ };

		std::variant< PBR, Lambertian, Mirror, Environment > brdf;
	};
	std::unordered_map< std::string, Material > materials;

	//textures referenced by materials:
	struct Texture {
		std::string src; //src used in the s72 file
		enum class Type {
			flat, //"2D" in the spec, but identifier can't start with a number
			cube,
		} type = Type::flat;
		enum class Format {
			linear,
			srgb,
			rgbe,
		} format = Format::linear;

		//computed during loading:
		std::string path; //path to data file, taking into account path to s72 file (relative to current working directory)
	};
	//we organize textures by src + type + format, so that two materials using to the same image *in the same way* end up referring to the same texture object:
	std::unordered_map< std::string, Texture > textures;

	//zero or more Environments, all with unique names:
	struct Environment {
		std::string name;

		Texture *radiance; //NOTE: always of type "cube"; will always be non-null once scene has been loaded
	};
	std::unordered_map< std::string, Environment > environments;

	//zero or more "LIGHT"s, all with unique names:
	struct Light {
		std::string name;

		color tint = color{ .r = 1.0f, .g = 1.0f, .b = 1.0f };

		uint32_t shadow = 0; //optional, if not set will be '0'

		//light has exactly one of these sources:
		struct Sun {
			float angle;
			float strength;
		};
		struct Sphere {
			float radius;
			float power;
			float limit = std::numeric_limits< float >::infinity(); //optional, will be infinity if not specified
		};
		struct Spot {
			float radius;
			float power;
			float limit = std::numeric_limits< float >::infinity(); //optional, will be infinity if not specified
			float fov;
			float blend;
		};
		std::variant< Sun, Sphere, Spot > source;
	};
	std::unordered_map< std::string, Light > lights;
	int sunNum = 0;
	int sphereNum = 0;
	int spotNum = 0;
};
