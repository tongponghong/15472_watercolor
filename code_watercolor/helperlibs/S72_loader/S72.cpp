#include "S72.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <fstream>
#include <unordered_map>

//functions that do the inverse of those in vk_enum_string_helper.h :

//used for mesh topologies:
VkPrimitiveTopology topology_to_VkPrimitiveTopology(std::string const &topology) {
	static std::map< std::string, VkPrimitiveTopology > const map = {
#define DO( X ) { #X, VK_PRIMITIVE_TOPOLOGY_ ## X }
		DO( POINT_LIST ),
		DO( LINE_LIST ),
		DO( LINE_STRIP ),
		DO( TRIANGLE_LIST ),
		DO( TRIANGLE_STRIP ),
		DO( TRIANGLE_FAN ),
		DO( LINE_LIST_WITH_ADJACENCY ),
		DO( LINE_STRIP_WITH_ADJACENCY ),
		DO( TRIANGLE_LIST_WITH_ADJACENCY ),
		DO( TRIANGLE_STRIP_WITH_ADJACENCY ),
		DO( PATCH_LIST ),
#undef DO
	};
	auto f = map.find(topology);
	if (f == map.end()) throw new std::runtime_error("Unrecognized topology \"" + topology + "\".");
	return f->second;
}


//used for index formats:
VkIndexType format_to_VkIndexType(std::string const &format) {
	static std::map< std::string, VkIndexType > const map = {
#define DO( X ) { #X, VK_INDEX_TYPE_ ## X }
		DO( UINT16 ),
		DO( UINT32 ),
		DO( UINT8 ),
		//not including: VK_INDEX_TYPE_NONE_KHR,
#undef DO
	};
	auto f = map.find(format);
	if (f == map.end()) throw new std::runtime_error("Unrecognized index format \"" + format + "\".");
	return f->second;
}


//used for vertex data formats:
VkFormat format_to_VkFormat(std::string const &format) {
	static std::map< std::string, VkFormat > const map = {
#define DO( X ) { #X, VK_FORMAT_ ## X }
		//formats for which VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT is always set.
		// see: https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#features-required-format-support
		DO( R8_UNORM ),
		DO( R8_SNORM ),
		DO( R8_UINT ),
		DO( R8_SINT ),
		DO( R8G8_UNORM ),
		DO( R8G8_SNORM ),
		DO( R8G8_UINT ),
		DO( R8G8_SINT ),

		DO( R8G8B8A8_UNORM ),
		DO( R8G8B8A8_SNORM ),
		DO( R8G8B8A8_UINT ),
		DO( R8G8B8A8_SINT ),
		DO( B8G8R8A8_UNORM ),

		DO( A8B8G8R8_UNORM_PACK32 ),
		DO( A8B8G8R8_SNORM_PACK32 ),
		DO( A8B8G8R8_UINT_PACK32 ),
		DO( A8B8G8R8_SINT_PACK32 ),

		DO( A2B10G10R10_UNORM_PACK32 ),

		DO( R16_UNORM ),
		DO( R16_SNORM ),
		DO( R16_UINT ),
		DO( R16_SINT ),
		DO( R16_SFLOAT ),
		DO( R16G16_UNORM ),
		DO( R16G16_SNORM ),
		DO( R16G16_UINT ),
		DO( R16G16_SINT ),
		DO( R16G16_SFLOAT ),
		DO( R16G16B16A16_UNORM ),
		DO( R16G16B16A16_SNORM ),
		DO( R16G16B16A16_UINT ),
		DO( R16G16B16A16_SINT ),
		DO( R16G16B16A16_SFLOAT ),

		DO( R32_UINT ),
		DO( R32_SINT ),
		DO( R32_SFLOAT ),
		DO( R32G32_UINT ),
		DO( R32G32_SINT ),
		DO( R32G32_SFLOAT ),
		DO( R32G32B32_UINT ),
		DO( R32G32B32_SINT ),
		DO( R32G32B32_SFLOAT ),
		DO( R32G32B32A32_UINT ),
		DO( R32G32B32A32_SINT ),
		DO( R32G32B32A32_SFLOAT ),
#undef DO
	};
	auto f = map.find(format);
	if (f == map.end()) throw new std::runtime_error("Unrecognized index format \"" + format + "\".");
	return f->second;
}


//helpers used in loading:

//warn if any members of an object haven't been handled (+ deleted):
void warn_on_unhandled(std::map< std::string, sejp::value > &object, std::string const &what) {
	if (object.empty()) return;
	std::cerr << "WARNING: " << what << " contained unhandled properties: ";
	bool first = true;
	for (auto const &[key, value] : object) {
		if (!first) std::cerr << ", ";
		first = false;
		std::cerr << key;
	}
	std::cerr << '.' << std::endl;
}

//pull out a string property of a sejp object as a string.
// throws if the property is missing
// deletes property from the object and returns the value if all is well
std::string extract_string(std::map< std::string, sejp::value > *object_, std::string const &key, std::string const &what) {
	assert(object_);
	auto &object = *object_;

	std::string string;
	try {
		string = object.at(key).as_string().value();
	} catch (std::exception &) {
		throw std::runtime_error(what + " is missing or not a string.");
	}
	object.erase(object.find(key));
	return string;
};

//pull out a number property of a sejp object as an uint32_t.
// throws if the property is missing or can't fit in a uint32_t
// deletes property from the object and returns the value if all is well
uint32_t extract_uint32_t(std::map< std::string, sejp::value > *object_, std::string const &key, std::string const &what) {
	assert(object_);
	auto &object = *object_;

	double number;
	try {
		number = object.at(key).as_number().value();
	} catch (std::exception &) {
		throw std::runtime_error(what + " is not a number.");
	}
	if (std::round(number) != number || number < 0.0 || number > double(std::numeric_limits< uint32_t >::max())) {
		throw std::runtime_error(what + " does not fit in an unsigned 32-bit integer.");
	}
	object.erase(object.find(key));
	return uint32_t(number);
};


//pull out a number property of a sejp object as a float.
// throws if the property is missing
// deletes property from the object and returns the value if all is well
float extract_float(std::map< std::string, sejp::value > *object_, std::string const &key, std::string const &what) {
	assert(object_);
	auto &object = *object_;

	double number;
	try {
		number = object.at(key).as_number().value();
	} catch (std::exception &) {
		throw std::runtime_error(what + " is not a number.");
	}
	object.erase(object.find(key));
	return float(number);
};


//pull out an array property of a sejp object as a vector of float.
// throws if the property is missing or contains non-number data
// deletes property from the object and returns the value if all is well
std::vector< float > extract_float_vector(std::map< std::string, sejp::value > *object_, std::string const &key, std::string const &what) {
	assert(object_);
	auto &object = *object_;

	std::vector< float > ret;
	try {
		std::vector< sejp::value > const &array = object.at(key).as_array().value();
		ret.reserve(array.size());
		for (sejp::value const &value : array) {
			ret.emplace_back(float(value.as_number().value()));
		}
	} catch (std::exception &) {
		throw std::runtime_error(what + " is not an array of numbers.");
	}
	object.erase(object.find(key));
	return ret;
};


//parse a texture map property of a sejp object into an S72's texture storage
// throws if the property is missing or doesn't parse as a texture
// deletes property from the object and returns a reference to the S72's textures container on success
S72::Texture &extract_map(std::map< std::string, sejp::value > *object_, std::string const &key, S72 *s72_, std::string const &what) {
	assert(object_);
	auto &object = *object_;
	assert(s72_);
	auto &s72 = *s72_;

	std::map< std::string, sejp::value > obj;
	try {
		obj = object.at(key).as_object().value();
	} catch (std::exception &) {
		throw std::runtime_error(what + " is not an object.");
	}

	std::string src = extract_string(&obj, "src", what + "'s src");
	S72::Texture::Type type = S72::Texture::Type::flat;
	if (obj.contains("type")) {
		static std::map< std::string, S72::Texture::Type > string_to_type{
			{"2D", S72::Texture::Type::flat},
			{"cube", S72::Texture::Type::cube},
		};

		std::string str = extract_string(&obj, "type", what + "'s type");
		try {
			type = string_to_type.at(str);
		} catch (std::exception &) {
			throw std::runtime_error(what + "'s type \"" + str + "\" is not a recognized texture type.");
		}
	}

	S72::Texture::Format format = S72::Texture::Format::linear;
	if (obj.contains("format")) {
		static std::map< std::string, S72::Texture::Format > string_to_format{
			{"linear", S72::Texture::Format::linear},
			{"srgb", S72::Texture::Format::srgb},
			{"rgbe", S72::Texture::Format::rgbe},
		};

		std::string str = extract_string(&obj, "format", what + "'s format");
		try {
			format = string_to_format.at(str);
		} catch (std::exception &) {
			throw std::runtime_error(what + "'s format \"" + str + "\" is not a recognized texture format.");
		}
	}

	warn_on_unhandled(obj, what);

	object.erase(object.find(key));

	std::string texture_key = src + ", format " + std::to_string(int(type)) + ", type " + std::to_string(int(format));

	return s72.textures.emplace(texture_key, S72::Texture{.src = src, .type = type, .format = format}).first->second;
}


S72 S72::load(std::string const &scene_file) {

	S72 s72; //the loaded scene, will be returned at end of function

	sejp::value json = sejp::load(scene_file);

	std::optional< std::vector< sejp::value > > array = json.as_array();

	//fetch top-level array from scene file:
	if (!array) {
		throw std::runtime_error("Top-level value of s72 file should be an array.");
	}

	//check magic value:
	if (!(array->size() >= 1 && array->at(0).as_string() && array->at(0).as_string().value() == "s72-v2")) {
		throw std::runtime_error("First element of s72 array should be \"s72-v2\".");
	}

	//parse the remaining objects;
	for (size_t i = 1; i < array->size(); i++) {

		//make a copy of the object and erase members as they are parsed:
		std::map< std::string, sejp::value > object;
		try {
			object = array->at(i).as_object().value();
		} catch (std::exception &) {
			throw std::runtime_error("Array element " + std::to_string(i) + " is not an object.");
		}

		//All objects must have a "type" and "name":
		std::string type = extract_string(&object, "type", "Object at index " + std::to_string(i) + "'s \"type\"");
		std::string name = extract_string(&object, "name", "Object at index " + std::to_string(i) + "'s \"name\"");

		if (type == "SCENE") {
			//reference to the object we are parsing into:
			Scene &scene = s72.scene;

			//check that we haven't already parsed the scene info:
			if (scene.name != "") {
				throw std::runtime_error("Multiple \"SCENE\" objects in s72 file.");
			}

			//mark scene as parsed:
			scene.name = name;

			//"roots" is optional:
			if (auto f = object.find("roots"); f != object.end()) {
				std::vector< std::string > refs;
				try {
					std::vector< sejp::value > const &vec = f->second.as_array().value();
					for (auto const &value : vec) {
						refs.emplace_back(value.as_string().value());
					}
				} catch (std::exception &) {
					throw std::runtime_error("Scene \"" + name + "\"'s roots are not an array of strings.");
				}
				scene.roots.reserve(refs.size());
				for (auto const &ref : refs) {
					scene.roots.emplace_back(&s72.nodes[ref]); //NOTE: creates new (empty) nodes if not yet parsed; will check for this later
				}
				object.erase(f);
			}
		} else if (type == "NODE") {
			//get a reference to the object we are parsing into:
			Node &node = s72.nodes[name];

			//check that we haven't already parsed this node's information:
			if (node.name != "") {
				throw std::runtime_error("Multiple \"NODE\" objects with name \"" + name + "\".");
			}

			//mark the node as parsed:
			node.name = name;

			if (auto f = object.find("translation"); f != object.end()) {
				try {
					std::vector< sejp::value > const &vec = f->second.as_array().value();
					node.translation = vec3{
						// .x = float(vec.at(0).as_number().value()),
						// .y = float(vec.at(1).as_number().value()),
						// .z = float(vec.at(2).as_number().value()),
						float(vec.at(0).as_number().value()),
						float(vec.at(1).as_number().value()),
						float(vec.at(2).as_number().value()),
					};
					if (vec.size() != 3) throw std::runtime_error("trailing values");
				} catch (std::exception &) {
					throw std::runtime_error("Node \"" + name + "\"'s translation should be an array of three numbers.");
				}
				object.erase(f);
			}

			if (auto f = object.find("rotation"); f != object.end()) {
				try {
					std::vector< sejp::value > const &vec = f->second.as_array().value();
					node.rotation = quat{
						// .x = float(vec.at(0).as_number().value()),
						// .y = float(vec.at(1).as_number().value()),
						// .z = float(vec.at(2).as_number().value()),
						// .w = float(vec.at(3).as_number().value()),
						float(vec.at(0).as_number().value()),
						float(vec.at(1).as_number().value()),
						float(vec.at(2).as_number().value()),
						float(vec.at(3).as_number().value()),
					};
					if (vec.size() != 4) throw std::runtime_error("trailing values");
				} catch (std::exception &) {
					throw std::runtime_error("Node \"" + name + "\"'s rotation should be an array of four numbers.");
				}
				object.erase(f);
			}

			if (auto f = object.find("scale"); f != object.end()) {
				try {
					std::vector< sejp::value > const &vec = f->second.as_array().value();
					node.scale = vec3{
						// .x = float(vec.at(0).as_number().value()),
						// .y = float(vec.at(1).as_number().value()),
						// .z = float(vec.at(2).as_number().value()),
						float(vec.at(0).as_number().value()),
						float(vec.at(1).as_number().value()),
						float(vec.at(2).as_number().value()),
					};
					if (vec.size() != 3) throw std::runtime_error("trailing values");
				} catch (std::exception &) {
					throw std::runtime_error("Node \"" + name + "\"'s scale should be an array of three numbers.");
				}
				object.erase(f);
			}

			if (auto f = object.find("children"); f != object.end()) {
				std::vector< std::string > refs;
				try {
					std::vector< sejp::value > const &vec = f->second.as_array().value();
					for (auto const &value : vec) {
						refs.emplace_back(value.as_string().value());
					}
				} catch (std::exception &) {
					throw std::runtime_error("Node \"" + name + "\"'s children should be an array of strings.");
				}
				//create/fixup pointers to other nodes:
				node.children.reserve(refs.size());
				for (auto const &ref : refs) {
					node.children.emplace_back(&s72.nodes[ref]);
				}
				object.erase(f);
			}

			if (auto f = object.find("mesh"); f != object.end()) {
				std::string ref;
				try {
					ref = f->second.as_string().value();
				} catch (std::exception &) {
					throw std::runtime_error("Node \"" + name + "\"'s mesh should be a string.");
				}
				node.mesh = &s72.meshes[ref];
				object.erase(f);
			}

			if (auto f = object.find("camera"); f != object.end()) {
				std::string ref;
				try {
					ref = f->second.as_string().value();
				} catch (std::exception &) {
					throw std::runtime_error("Node \"" + name + "\"'s camera should be a string.");
				}
				node.camera = &s72.cameras[ref];
				object.erase(f);
			}

			if (auto f = object.find("environment"); f != object.end()) {
				std::string ref;
				try {
					ref = f->second.as_string().value();
				} catch (std::exception &) {
					throw std::runtime_error("Node \"" + name + "\"'s environment should be a string.");
				}
				node.environment = &s72.environments[ref];
				object.erase(f);
			}

			if (auto f = object.find("light"); f != object.end()) {
				std::string ref;
				try {
					ref = f->second.as_string().value();
				} catch (std::exception &) {
					throw std::runtime_error("Node \"" + name + "\"'s light should be a string.");
				}
				node.light = &s72.lights[ref];

				if (std::holds_alternative< S72::Light::Sun > (node.light->source)) {
					s72.sunNum++;
				}
				else if (std::holds_alternative< S72::Light::Sphere > (node.light->source)) {
					s72.sphereNum++;
				}
				else if (std::holds_alternative< S72::Light::Spot > (node.light->source)) {
					std::cout << "Added one to spotNum" << std::endl;
					s72.spotNum++;
				}
				object.erase(f);

			}

		} else if (type == "MESH"){
			//reference to the thing we are parsing into:
			Mesh &mesh = s72.meshes[name];

			//check that we haven't already parsed this:
			if (mesh.name != "") {
				throw std::runtime_error("Multiple \"MESH\" objects with name \"" + name + "\".");
			}

			//mark as parsed:
			mesh.name = name;

			std::string topology = extract_string(&object, "topology", "Mesh \"" + name + "\"'s topology");
			mesh.topology = topology_to_VkPrimitiveTopology(topology);

			mesh.count = extract_uint32_t(&object, "count", "Mesh \"" + name + "\"'s count");

			if (auto f = object.find("indices"); f != object.end()) {
				std::map< std::string, sejp::value > obj;
				try {
					obj = f->second.as_object().value();
				} catch (std::exception &) {
					throw std::runtime_error("Mesh \"" + name + "\"'s indices should be an object.");
				}

				std::string src = extract_string(&obj, "src", "Mesh \"" + name + "\"'s indices.src");
				uint32_t offset = extract_uint32_t(&obj, "offset", "Mesh \"" + name + "\"'s indices.offset");
				std::string format = extract_string(&obj, "format", "Mesh \"" + name + "\"'s indices.format");
				mesh.indices.emplace(Mesh::Indices{
					.src = s72.data_files[src],
					.offset = offset,
					.format = format_to_VkIndexType(format),
				});

				warn_on_unhandled(obj, "Mesh \"" + name + "\"'s indices");

				object.erase(f);
			}

			std::map< std::string, sejp::value > attributes;
			try {
				attributes = object.at("attributes").as_object().value();
			} catch (std::exception &) {
				throw std::runtime_error("Mesh \"" + name + "\"'s attributes should be an object.");
			}
			object.erase(object.find("attributes"));

			for (auto const &[key, value] : attributes) {
				std::map< std::string, sejp::value > obj;
				try {
					obj = value.as_object().value();
				} catch (std::exception &) {
					throw std::runtime_error("Mesh \"" + name + "\"'s attribute \"" + key + "\" is not an object.");
				}

				std::string src = extract_string(&obj, "src", "Mesh \"" + name + "\"'s attribute \"" + key + "\"'s src");
				uint32_t offset = extract_uint32_t(&obj, "offset", "Mesh \"" + name + "\"'s attribute \"" + key + "\"' offset");
				uint32_t stride = extract_uint32_t(&obj, "stride", "Mesh \"" + name + "\"'s attribute \"" + key + "\"' stride");
				std::string format = extract_string(&obj, "format", "Mesh \"" + name + "\"'s attribute \"" + key + "\"'s format");
				mesh.attributes.emplace(key, Mesh::Attribute{
					.src = s72.data_files[src],
					.offset = offset,
					.stride = stride,
					.format = format_to_VkFormat(format),
				});

				warn_on_unhandled(obj, "Mesh \"" + name + "\"'s attribute \"" + key + "\"");
			}

			if (auto f = object.find("material"); f != object.end()) {
				std::string material;
				try {
					material = f->second.as_string().value();
				} catch (std::exception &) {
					throw std::runtime_error("Mesh \"" + name + "\"'s material is not a string.");
				}
				object.erase(f);
				mesh.material = &s72.materials[material];
			}

		} else if (type == "CAMERA") {
			Camera &camera = s72.cameras[name];

			//check that we haven't already parsed this:
			if (camera.name != "") {
				throw std::runtime_error("Multiple \"CAMERA\" objects with name \"" + name + "\".");
			}

			//mark as parsed:
			camera.name = name;

			bool have_projection = false;

			if (auto f = object.find("perspective"); f != object.end()) {
				//make sure there aren't multiple projections on this camera:
				if (have_projection) {
					throw std::runtime_error("Camera \"" + name + "\" has multiple projections.");
				}
				have_projection = true;

				std::map< std::string, sejp::value > obj;
				try {
					obj = f->second.as_object().value();
				} catch (std::exception &) {
					throw std::runtime_error("Camera \"" + name + "\"'s projection is not an object.");
				}

				Camera::Perspective perspective;

				perspective.aspect = extract_float(&obj, "aspect", "Camera \"" + name + "\"'s projection.aspect");
				perspective.vfov = extract_float(&obj, "vfov", "Camera \"" + name + "\"'s projection.vfov");
				perspective.near = extract_float(&obj, "near", "Camera \"" + name + "\"'s projection.near");
				if (obj.contains("far")) {
					perspective.far = extract_float(&obj, "far", "Camera \"" + name + "\"'s projection.far");
				}

				camera.projection = perspective;

				warn_on_unhandled(obj, "Material \"" + name + "\"'s perspective");

				object.erase(f);
			}

			if (!have_projection) {
				throw std::runtime_error("Camera \"" + name + "\" does not have a projection.");
			}

		} else if (type == "DRIVER") {
			//NOTE: not building into an existing object because we need to have a node reference ready when constructing.

			std::string node = extract_string(&object, "node", "Driver \"" + name + "\"'s node");

			Driver::Channel channel;
			{ //get channel:
				static std::map< std::string, Driver::Channel > string_to_channel{
					{"translation", Driver::Channel::translation},
					{"rotation", Driver::Channel::rotation},
					{"scale", Driver::Channel::scale},
				};

				std::string str = extract_string(&object, "channel", "Driver \"" + name + "\"'s channel");
				try {
					channel = string_to_channel.at(str);
				} catch (std::exception &) {
					throw std::runtime_error("Driver \"" + name + "\"'s channel \"" + str + "\" is not a recognized channel name.");
				}
			}

			std::vector< float > times = extract_float_vector(&object, "times", "Driver \"" + name + "\"'s times");
			for (size_t t = 1; t < times.size(); ++t) {
				if (times[t-1] > times[t]) {
					throw std::runtime_error("Driver \"" + name + "\"'s times are not non-decreasing.");
				}
			}

			std::vector< float > values = extract_float_vector(&object, "values", "Driver \"" + name + "\"'s values");

			//check that times/values counts are consistent with channel type:
			if (channel == Driver::Channel::translation || channel == Driver::Channel::scale) {
				if (times.size() * 3 != values.size()) {
					throw std::runtime_error("Driver \"" + name + "\" doesn't have times * 3 values.");
				}
			} else if (channel == Driver::Channel::rotation) {
				if (times.size() * 4 != values.size()) {
					throw std::runtime_error("Driver \"" + name + "\" doesn't have times * 4 values.");
				}
			} else {
				assert(0 && "unreachable case");
			}

			Driver::Interpolation interpolation = Driver::Interpolation::LINEAR;
			if(object.contains("interpolation")){
				static std::map< std::string, Driver::Interpolation > string_to_interpolation{
					{"STEP", Driver::Interpolation::STEP},
					{"LINEAR", Driver::Interpolation::LINEAR},
					{"SLERP", Driver::Interpolation::SLERP},
				};

				std::string str = extract_string(&object, "interpolation", "Driver \"" + name + "\"'s interpolation");
				try {
					interpolation = string_to_interpolation.at(str);
				} catch (std::exception &) {
					throw std::runtime_error("Driver \"" + name + "\"'s interpolation \"" + str + "\" is not a recognized interpolation name.");
				}
			}

			s72.drivers.emplace_back(Driver{
				.name = name,
				.node = s72.nodes[node],
				.channel = channel,
				.times = std::move(times),
				.values = std::move(values),
				.interpolation = interpolation,
			});
		} else if (type == "MATERIAL") {
			Material &material = s72.materials[name];

			//check that we haven't already parsed this:
			if (material.name != "") {
				throw std::runtime_error("Multiple \"MATERIAL\" objects with name \"" + name + "\".");
			}

			//mark as parsed:
			material.name = name;

			if (object.contains("normalMap")) {
				material.normal_map = &extract_map(&object, "normalMap", &s72, "Material \"" + name + "\"'s normalMap");
			}
			if (object.contains("displacementMap")) {
				material.displacement_map = &extract_map(&object, "displacementMap", &s72, "Material \"" + name + "\"'s displacementMap");
			}

			bool have_brdf = false;

			if (auto b = object.find("pbr"); b != object.end()) {
				if (have_brdf) {
					throw std::runtime_error("Material \"" + name + "\" has multiple brdfs.");
				}
				have_brdf = true;

				std::map< std::string, sejp::value > obj;
				try {
					obj = b->second.as_object().value();
				} catch (std::exception &) {
					throw std::runtime_error("Material \"" + name + "\"'s pbr is not an object.");
				}

				Material::PBR pbr;

				if (auto f = obj.find("albedo"); f != obj.end()) {
					if (std::optional< std::vector< sejp::value > > arr = f->second.as_array()) {
						//3-vector of color
						try {
							std::vector< sejp::value > const &vec = arr.value();
							pbr.albedo = color{
								.r = float(vec.at(0).as_number().value()),
								.g = float(vec.at(1).as_number().value()),
								.b = float(vec.at(2).as_number().value()),
							};
							if (vec.size() != 3) throw std::runtime_error("trailing values");
						} catch (std::exception &) {
							throw std::runtime_error("Material \"" + name + "\"'s pbr.albedo was an array but it didn't hold exactly three numbers.");
						}
						obj.erase(f);
					} else {
						pbr.albedo = &extract_map(&obj, "albedo", &s72, "Material \"" + name + "\"'s pbr.albedo");
					}
				}

				if (auto f = obj.find("roughness"); f != obj.end()) {
					if (std::optional< double > number = f->second.as_number()) {
						pbr.roughness = float(number.value());
						obj.erase(f);
					} else {
						pbr.roughness = &extract_map(&obj, "roughness", &s72, "Material \"" + name + "\"'s pbr.roughness");
					}
				}

				if (auto f = obj.find("metalness"); f != obj.end()) {
					if (std::optional< double > number = f->second.as_number()) {
						pbr.metalness = float(number.value());
						obj.erase(f);
					} else {
						pbr.metalness = &extract_map(&obj, "metalness", &s72, "Material \"" + name + "\"'s pbr.metalness");
					}
				}

				material.brdf = pbr;

				warn_on_unhandled(obj, "Material \"" + name + "\"'s pbr");

				object.erase(b);
			}

			if (auto b = object.find("lambertian"); b != object.end()) {
				if (have_brdf) {
					throw std::runtime_error("Material \"" + name + "\" has multiple brdfs.");
				}
				have_brdf = true;

				std::map< std::string, sejp::value > obj;
				try {
					obj = b->second.as_object().value();
				} catch (std::exception &) {
					throw std::runtime_error("Material \"" + name + "\"'s lambertian is not an object.");
				}

				Material::Lambertian lambertian;

				if (auto f = obj.find("albedo"); f != obj.end()) {
					if (std::optional< std::vector< sejp::value > > arr = f->second.as_array()) {
						//3-vector of color
						try {
							std::vector< sejp::value > const &vec = arr.value();
							lambertian.albedo = color{
								.r = float(vec.at(0).as_number().value()),
								.g = float(vec.at(1).as_number().value()),
								.b = float(vec.at(2).as_number().value()),
							};
							if (vec.size() != 3) throw std::runtime_error("trailing values");
						} catch (std::exception &) {
							throw std::runtime_error("Material \"" + name + "\"'s lambertian.albedo was an array but it didn't hold exactly three numbers.");
						}
						obj.erase(f);
					} else {
						lambertian.albedo = &extract_map(&obj, "albedo", &s72, "Material \"" + name + "\"'s lambertian.albedo");
					}
				}

				material.brdf = lambertian;

				warn_on_unhandled(obj, "Material \"" + name + "\"'s lambertian");

				object.erase(b);
			}

			if (auto b = object.find("mirror"); b != object.end()) {
				if (have_brdf) {
					throw std::runtime_error("Material \"" + name + "\" has multiple brdfs.");
				}
				have_brdf = true;

				std::map< std::string, sejp::value > obj;
				try {
					obj = b->second.as_object().value();
				} catch (std::exception &) {
					throw std::runtime_error("Material \"" + name + "\"'s mirror is not an object.");
				}

				Material::Mirror mirror;

				//no properties!

				material.brdf = mirror;

				warn_on_unhandled(obj, "Material \"" + name + "\"'s mirror");

				object.erase(b);
			}

			if (auto b = object.find("environment"); b != object.end()) {
				if (have_brdf) {
					throw std::runtime_error("Material \"" + name + "\" has multiple brdfs.");
				}
				have_brdf = true;

				std::map< std::string, sejp::value > obj;
				try {
					obj = b->second.as_object().value();
				} catch (std::exception &) {
					throw std::runtime_error("Material \"" + name + "\"'s environment is not an object.");
				}

				Material::Environment environment;

				//no properties!

				material.brdf = environment;

				warn_on_unhandled(obj, "Material \"" + name + "\"'s environment");

				object.erase(b);
			}

			if (!have_brdf) {
				throw std::runtime_error("Material \"" + name + "\" does not have a brdf.");
			}

		} else if (type == "ENVIRONMENT"){
			Environment &environment = s72.environments[name];

			//check that we haven't already parsed this:
			if (environment.name != "") {
				throw std::runtime_error("Multiple \"ENVIRONMENT\" objects with name \"" + name + "\".");
			}

			//mark as parsed:
			environment.name = name;

			environment.radiance = &extract_map(&object, "radiance", &s72, "Environment \"" + name + "\"'s radiance");

			if (environment.radiance->type != Texture::Type::cube) {
				throw std::runtime_error("Environment \"" + name + "\"'s radiance is not a cube.");
			}

		} else if (type == "LIGHT") {
			Light &light = s72.lights[name];

			//check that we haven't already parsed this:
			if (light.name != "") {
				throw std::runtime_error("Multiple \"LIGHT\" objects with name \"" + name + "\".");
			}

			//mark as parsed:
			light.name = name;

			if (auto f = object.find("tint"); f != object.end()) {
				try {
					std::vector< sejp::value > const &vec = f->second.as_array().value();
					light.tint = color{
						.r = float(vec.at(0).as_number().value()),
						.g = float(vec.at(1).as_number().value()),
						.b = float(vec.at(2).as_number().value()),
					};
					if (vec.size() != 3) throw std::runtime_error("trailing values");
				} catch (std::exception &) {
					throw std::runtime_error("Light \"" + name + "\"'s tint was not an array of three numbers.");
				}
				object.erase(f);
			}
			if (object.contains("shadow")){
				light.shadow = extract_uint32_t(&object, "shadow", "Light \"" + name + "\"'s shadow");
			}

			bool have_source = false;

			if (auto f = object.find("sun"); f != object.end()) {
				if (have_source) {
					throw std::runtime_error("Light \"" + name + "\" has multiple sources.");
				}
				have_source = true;

				std::map< std::string, sejp::value > obj;
				try {
					obj = f->second.as_object().value();
				} catch (std::exception &) {
					throw std::runtime_error("Light \"" + name + "\"'s sun is not an object.");
				}

				Light::Sun sun;

				sun.angle = extract_float(&obj, "angle", "Light \"" + name + "\"'s sun's angle");
				sun.strength = extract_float(&obj, "strength", "Light \"" + name + "\"'s sun's strength");

				light.source = sun;
				//s72.sunNum++;

				warn_on_unhandled(obj, "Light \"" + name + "\"'s sun");
				object.erase(f);
			}
			if (auto f = object.find("sphere"); f != object.end()) {
				if (have_source) {
					throw std::runtime_error("Light \"" + name + "\" has multiple sources.");
				}
				have_source = true;

				std::map< std::string, sejp::value > obj;
				try {
					obj = f->second.as_object().value();
				} catch (std::exception &) {
					throw std::runtime_error("Light \"" + name + "\"'s sphere is not an object.");
				}

				Light::Sphere sphere;

				sphere.radius = extract_float(&obj, "radius", "Light \"" + name + "\"'s sphere's radius");
				sphere.power = extract_float(&obj, "power", "Light \"" + name + "\"'s sphere's power");
				if (obj.contains("limit")) {
					sphere.limit = extract_float(&obj, "limit", "Light \"" + name + "\"'s sphere's limit");
				}

				light.source = sphere;
				//s72.sphereNum++;

				warn_on_unhandled(obj, "Light \"" + name + "\"'s sphere");
				object.erase(f);
			}
			if (auto f = object.find("spot"); f != object.end()) {
				if (have_source) {
					throw std::runtime_error("Light \"" + name + "\" has multiple sources.");
				}
				have_source = true;

				std::map< std::string, sejp::value > obj;
				try {
					obj = f->second.as_object().value();
				} catch (std::exception &) {
					throw std::runtime_error("Light \"" + name + "\"'s spot is not an object.");
				}

				Light::Spot spot;

				spot.radius = extract_float(&obj, "radius", "Light \"" + name + "\"'s spot's radius");
				spot.power = extract_float(&obj, "power", "Light \"" + name + "\"'s spot's power");
				if (obj.contains("limit")) {
					spot.limit = extract_float(&obj, "limit", "Light \"" + name + "\"'s spot's limit");
				}
				spot.fov = extract_float(&obj, "fov", "Light \"" + name + "\"'s spot's fov");
				spot.blend = extract_float(&obj, "blend", "Light \"" + name + "\"'s spot's blend");

				light.source = spot;
				//s72.spotNum++;
			
				warn_on_unhandled(obj, "Light \"" + name + "\"'s spot");
				object.erase(f);
			}

			if (!have_source) {
				throw std::runtime_error("Light \"" + name + "\" is missing a source.");
			}
		} else {
			std::cerr << "WARNING: ignoring object \"" << name << "\" of unrecognized type \"" << type << "\"." << std::endl;
		}
		warn_on_unhandled(object, "Object \"" + name + "\"");
	}


	//-----------------------------------------------------------------------
	//fix up paths for Datafiles and Textures to be relative to the s72 file

	std::string scene_folder = "";
	{ //extract prefix for relative paths:
		auto pos = scene_file.find_last_of("\\/");
		if (pos != std::string::npos) {
			scene_folder = scene_file.substr(0 , pos+1);
		}
	}

	//data files are just empty objects, but in a map with keys = src:
	for (auto &[key, value] : s72.data_files) {
		value.src = key;
		value.path = scene_folder + value.src;
	}

	//textures are already populated with src, type, format; just need to set path:
	for (auto &[key, value] : s72.textures) {
		value.path = scene_folder + value.src;
	}

	return s72;
}
