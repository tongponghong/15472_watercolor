#include "RTG.hpp"

#include "VK.hpp"

#include <vulkan/vulkan_core.h>
#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h> //for portability subset
#include <vulkan/vulkan_metal.h> //for VK_EXT_METAL_SURFACE_EXTENSION_NAME
#endif
#include <vulkan/vk_enum_string_helper.h> //useful for debug output
#include <vulkan/utility/vk_format_utils.h> //for getting format sizes
#include <GLFW/glfw3.h>
#include "helperlibs/S72_loader/S72.hpp"

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>

// consider using &argc and &argv so that it comes in as a list of arguments to be removed from
void RTG::Configuration::parse(int argc, char **argv) {
	for (int argi = 1; argi < argc; ++argi) {
		std::string arg = argv[argi];
		if (arg == "--debug") {
			debug = true;
		} else if (arg == "--no-debug") {
			debug = false;
		} else if (arg == "--physical-device") {
			if (argi + 1 >= argc) throw std::runtime_error("--physical-device requires a parameter (a device name).");
			argi += 1;
			physical_device_name = argv[argi];
		} else if (arg == "--drawing-size") {
			if (argi + 2 >= argc) throw std::runtime_error("--drawing-size requires two parameters (width and height).");
			auto conv = [&](std::string const &what) {
				argi += 1;
				std::string val = argv[argi];
				for (size_t i = 0; i < val.size(); ++i) {
					if (val[i] < '0' || val[i] > '9') {
						throw std::runtime_error("--drawing-size " + what + " should match [0-9]+, got '" + val + "'.");
					}
				}
				return std::stoul(val);
			};
			surface_extent.width = conv("width");
			surface_extent.height = conv("height");
		} else if (arg == "--headless") {
			headless = true;
		} else if (arg == "--scene") {
			argi += 1;
			scene_path = argv[argi];
			std::cout << "This is the scene you entered: " << scene_path << std::endl;
		} else if (arg == "--camera") {
			argi += 1;
			user_entered_camera = argv[argi]; 

		} else if (arg == "--culling") {

		} else if (arg == "--lambertian") {
			argi += 1;
			lamb_lookup_tex_path = argv[argi];
			std::cout << "This is the lambertian lookup you entered: " << lamb_lookup_tex_path << std::endl;

		} else if (arg == "--exposure") {
			argi += 1;
			exp_val = argv[argi];
			std::cout << "This is the exposure value you entered: " << exp_val << std::endl;

		} else if (arg == "--tone-map") {
			argi += 1;
			user_tonemap_type = argv[argi];
			std::cout << "This is the tonemap you entered: " << user_tonemap_type << std::endl;
		}
		
		else {
			throw std::runtime_error("Unrecognized argument '" + arg + "'.");
		}
	}
}

void RTG::Configuration::usage(std::function< void(const char *, const char *) > const &callback) {
	callback("--debug, --no-debug", "Turn on/off debug and validation layers.");
	callback("--physical-device <name>", "Run on the named physical device (guesses, otherwise).");
	callback("--drawing-size <w> <h>", "Set the size of the surface to draw to.");
	callback("--headless", "Don't create a window; read events from stdin.");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT *data,
	void *user_data
) {
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		std::cerr << "\x1b[91m" << "E: ";
	} else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		std::cerr << "\x1b[33m" << "w: ";
	} else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		std::cerr << "\x1b[90m" << "i: ";
	} else { //VERBOSE
		std::cerr << "\x1b[90m" << "v: ";
	}
	std::cerr << data->pMessage << "\x1b[0m" << std::endl;

	return VK_FALSE;
}

RTG::RTG(Configuration const &configuration_) : helpers(*this) {

	//copy input configuration:
	// configuration = configuration_;
	configuration = configuration_;

	{ // get the scene
		try {
			scene = S72::load(configuration.scene_path);
		} catch (std::exception &e) {
			std::cerr << "Failed loading scene\n" << e.what() << std::endl;
			exit(1);
		}
	}

	{ // check if camera exists
		if (configuration.user_entered_camera != "" &&
			scene.cameras.find(configuration.user_entered_camera) == scene.cameras.end()) {
			std::cerr << "Failed to find camera" << std::endl;
			exit(1);
		}
		else {
			userEnteredCamera = configuration.user_entered_camera;
		}
	}

	{
		lamb_lookup_tex = configuration.lamb_lookup_tex_path;
	}
	exposure = std::stof(configuration.exp_val);

	{
		if (configuration.user_tonemap_type != "" &&
			map_tonemaps.find(configuration.user_tonemap_type) == map_tonemaps.end()) {
			std::cerr << "Failed to find tonemap" << std::endl;
			exit(1);
		}
		else {
			tonemap_type = map_tonemaps[configuration.user_tonemap_type];
		}
	}
	
	// //fill in flags/extensions/layers information:

	{ //create the `instance` (main handle to Vulkan library):
		VkInstanceCreateFlags instance_flags = 0;
		std::vector< const char * > instance_extensions;
		std::vector< const char * > instance_layers;

		//add extensions for MoltenVK portability layer on macOS
		#if defined(__APPLE__)
		instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

		instance_extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		instance_extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
		instance_extensions.emplace_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
		#endif

		//add extensions and layers for debugging:
		if (configuration.debug) {
			instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			instance_layers.emplace_back("VK_LAYER_KHRONOS_validation");
		}

		// GLFW is the library used to get the window
		if(!configuration.headless) { //add extensions needed by glfw:
			glfwInit();
			if (!glfwVulkanSupported()) {
				throw std::runtime_error("GLFW reports Vulkan is not supported.");
			}

			uint32_t count;
			const char **extensions = glfwGetRequiredInstanceExtensions(&count);
			if (extensions == nullptr) {
				throw std::runtime_error("GLFW failed to return a list of requested instance extensions. Perhaps it was not compiled with Vulkan support.");
			}
			for (uint32_t i = 0; i < count; ++i) {
				instance_extensions.emplace_back(extensions[i]);
			}
		}

		// write debug messenger structure
		VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = debug_callback,
			.pUserData = nullptr
		};

		VkInstanceCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = (configuration.debug ? &debug_messenger_create_info : nullptr), // pass debug structure if configured
			.flags = instance_flags,
			.pApplicationInfo = &configuration.application_info,
			.enabledLayerCount = uint32_t(instance_layers.size()),
			.ppEnabledLayerNames = instance_layers.data(),
			.enabledExtensionCount = uint32_t(instance_extensions.size()),
			.ppEnabledExtensionNames = instance_extensions.data()
		};
		VK( vkCreateInstance(&create_info, nullptr, &instance) );

		// create debug messenger
		if (configuration.debug) {
			PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (!vkCreateDebugUtilsMessengerEXT) {
				throw std::runtime_error("Failed to lookup debug utils create fn.");
			}
			VK( vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, nullptr, &debug_messenger) );
		}
	}

	if (!configuration.headless) { //create the `window` and `surface` (where things get drawn):
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(configuration.surface_extent.width, configuration.surface_extent.height, configuration.application_info.pApplicationName, nullptr, nullptr);

		if (!window) {
			throw std::runtime_error("GLFW failed to create a window.");
		}

		VK( glfwCreateWindowSurface(instance, window, nullptr, &surface) );
	}


	{ //select the `physical_device` -- the gpu that will be used to draw:
		std::vector< std::string > physical_device_names; //for later error message
		{ //pick a physical device
			uint32_t count = 0;
			VK( vkEnumeratePhysicalDevices(instance, &count, nullptr) );
			std::vector< VkPhysicalDevice > physical_devices(count);
			VK( vkEnumeratePhysicalDevices(instance, &count, physical_devices.data()) );

			uint32_t best_score = 0;

			for (auto const &pd : physical_devices) {
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(pd, &properties);

				VkPhysicalDeviceFeatures features;
				vkGetPhysicalDeviceFeatures(pd, &features);

				physical_device_names.emplace_back(properties.deviceName);

				if (!configuration.physical_device_name.empty()) {
					if (configuration.physical_device_name == properties.deviceName) {
						if (physical_device) {
							std::cerr << "WARNING: have two physical devices with the name '" << properties.deviceName << "'; using the first to be enumerated." << std::endl;
						} 
						else {
							physical_device = pd;
						}
					}
				} 
				else {
					uint32_t score = 1;
					if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
						score += 0x8000;
					}

					if (score > best_score) {
						best_score = score;
						physical_device = pd;
					}
				}
			}
		}

		if (physical_device == VK_NULL_HANDLE) {
			std::cerr << "Physical devices:\n";
			for (std::string const &name : physical_device_names) {
				std::cerr << "    " << name << "\n";
			}
			std::cerr.flush();

			if (!configuration.physical_device_name.empty()) {
				throw std::runtime_error("No physical device with name '" + configuration.physical_device_name + "'.");
			} 
			else {
				throw std::runtime_error("No suitable GPU found.");
			}
		}

		{ //report device name:
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physical_device, &properties);
			std::cout << "Selected physical device '" << properties.deviceName << "'." << std::endl;
		}
	}
	

	{ //select the `surface_format` and `present_mode` which control how colors are represented on the surface and how new images are supplied to the surface:
		if (configuration.headless) {
			//in headless mode, just use the first requested format:
			if (configuration.surface_formats.empty()) {
				throw std::runtime_error("No surface formats requested.");
			}
			surface_format = configuration.surface_formats[0];

			//headless mode will always use VK_PRESENT_MODE_FIFO_KHR, so make sure that's an option:
			bool have_fifo = false;
			for (auto const &mode : configuration.present_modes) {
				if (mode == VK_PRESENT_MODE_FIFO_KHR) {
					have_fifo = true;
					break;
				}
			}
			if (!have_fifo) {
				throw std::runtime_error("Configured present modes do not contain VK_PRESENT_MODE_FIFO_KHR.");
			}
			present_mode = VK_PRESENT_MODE_FIFO_KHR;

			present_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		}
		
		else {
			std::vector< VkSurfaceFormatKHR > formats;
			std::vector< VkPresentModeKHR > present_modes;
			
			{
				uint32_t count = 0;
				VK( vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, nullptr) );
				formats.resize(count);
				VK( vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, formats.data()) );
			}

			{
				uint32_t count = 0;
				VK( vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, nullptr) );
				present_modes.resize(count);
				VK( vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, present_modes.data()) );
			}

			//find first available surface format matching config:
			surface_format = [&](){
				std::cout << "Supported surface formats:" << std::endl;
				uint32_t surface_format_ct = 0;

				for (auto const &config_format : configuration.surface_formats) {
					for (auto const &format : formats) {
						std::cout << "[" << surface_format_ct << "]" << string_VkFormat(format.format) << std::endl;
						surface_format_ct++;	
					}

					for (auto const &format : formats) {
						if (config_format.format == format.format && config_format.colorSpace == format.colorSpace) {
							return format;
						}
					}
				}
				throw std::runtime_error("No format matching requested format(s) found.");
			}();

			//find first available present mode matching config:
			present_mode = [&](){
				std::cout << "Supported present modes:" << std::endl;
				uint32_t present_mode_ct = 0;
				for (auto const &config_mode : configuration.present_modes) {
					for (auto const &mode : present_modes) {
						std::cout << "[" << present_mode_ct << "]" << string_VkPresentModeKHR(mode) << std::endl;
						present_mode_ct++;
					}

					for (auto const &mode : present_modes) {
						if (config_mode == mode) {
							return mode;
						}
					}
				}
				throw std::runtime_error("No present mode matching requested mode(s) found.");
			}();

			present_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}
	}

	{ //create the `device` (logical interface to the GPU) and the `queue`s to which we can submit commands:
		{ //look up queue indices:
			uint32_t count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
			std::vector< VkQueueFamilyProperties > queue_families(count);
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queue_families.data());

			for (auto const &queue_family : queue_families) {
				uint32_t i = uint32_t(&queue_family - &queue_families[0]);

				//if it does graphics, set the graphics queue family:
				if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					if (!graphics_queue_family) graphics_queue_family = i;
				}

				if (!configuration.headless) {
					//if it has present support, set the present queue family:
					VkBool32 present_support = VK_FALSE;
					VK( vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support) );

					if (present_support == VK_TRUE) {
						if (!present_queue_family) present_queue_family = i;
					}
				}
			}

			//in headless mode, "present" (copy-to-host) on the graphics queue:
			if (configuration.headless) {
				present_queue_family = graphics_queue_family;
			}

			if (!graphics_queue_family) {
				throw std::runtime_error("No queue with graphics support.");
			}

			if (!present_queue_family) {
				throw std::runtime_error("No queue with present support.");
			}
		}

		//select device extensions:
		std::vector< const char * > device_extensions;
		#if defined(__APPLE__)
		device_extensions.emplace_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
		#endif
		//Add the swapchain extension:
		if (!configuration.headless) {
			device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}

		{ //create the logical device:
			std::vector< VkDeviceQueueCreateInfo > queue_create_infos;
			std::set< uint32_t > unique_queue_families{
				graphics_queue_family.value(),
				present_queue_family.value()
			};

			float queue_priorities[1] = { 1.0f };
			for (uint32_t queue_family : unique_queue_families) {
				queue_create_infos.emplace_back(VkDeviceQueueCreateInfo{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = queue_family,
					.queueCount = 1,
					.pQueuePriorities = queue_priorities,
				});
			}

			VkPhysicalDeviceFeatures device_features {
				.multiViewport = VK_TRUE,
			};

			VkDeviceCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.queueCreateInfoCount = uint32_t(queue_create_infos.size()),
				.pQueueCreateInfos = queue_create_infos.data(),

				//device layers are depreciated; spec suggests passing instance_layers or nullptr:
				.enabledLayerCount = 0,
				.ppEnabledLayerNames = nullptr,

				.enabledExtensionCount = static_cast< uint32_t>(device_extensions.size()),
				.ppEnabledExtensionNames = device_extensions.data(),

				//pass a pointer to a VkPhysicalDeviceFeatures to request specific features: (e.g., thick lines)
				.pEnabledFeatures = &device_features,
			};

			VK( vkCreateDevice(physical_device, &create_info, nullptr, &device) );

			vkGetDeviceQueue(device, graphics_queue_family.value(), 0, &graphics_queue);
			vkGetDeviceQueue(device, present_queue_family.value(), 0, &present_queue);
		}
	}

	//run any resource creation required by Helpers structure:
	helpers.create();

	//create initial swapchain:
	recreate_swapchain();

	//create workspace resources:
	workspaces.resize(configuration.workspaces);
	for (auto &workspace : workspaces) {
		{ //create workspace fences:
			VkFenceCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.flags = VK_FENCE_CREATE_SIGNALED_BIT, //start signaled, because all workspaces are available to start
			};

			VK( vkCreateFence(device, &create_info, nullptr, &workspace.workspace_available) );
		}

		{ //create workspace semaphores:
			VkSemaphoreCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			};

			VK( vkCreateSemaphore(device, &create_info, nullptr, &workspace.image_available) );
		}
	}
}

RTG::~RTG() {
	//don't destroy until device is idle:
	if (device != VK_NULL_HANDLE) {
		if (VkResult result = vkDeviceWaitIdle(device); result != VK_SUCCESS) {
			std::cerr << "Failed to vkDeviceWaitIdle in RTG::~RTG [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
		}
	}

	//destroy workspace resources:
	for (auto &workspace : workspaces) {
		if (workspace.workspace_available != VK_NULL_HANDLE) {
			vkDestroyFence(device, workspace.workspace_available, nullptr);
			workspace.workspace_available = VK_NULL_HANDLE;
		}
		if (workspace.image_available != VK_NULL_HANDLE) {
			vkDestroySemaphore(device, workspace.image_available, nullptr);
			workspace.image_available = VK_NULL_HANDLE;
		}
	}
	workspaces.clear();

	//destroy the swapchain:
	destroy_swapchain();

	//destroy Helpers structure resources:
	helpers.destroy();

	//destroy the rest of the resources:
	if (device != VK_NULL_HANDLE) {
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}

	if (surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
	}

	if (window != nullptr) {
		glfwDestroyWindow(window);
		window = nullptr;
	}

	if (debug_messenger != VK_NULL_HANDLE) {
		PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (vkDestroyDebugUtilsMessengerEXT) {
			vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
			debug_messenger = VK_NULL_HANDLE;
		}
	}

	if (instance != VK_NULL_HANDLE) {
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}

}


void RTG::recreate_swapchain() {
	// clean up swapchain if exists
	if (!swapchain_images.empty()) {
		destroy_swapchain(); 
	}
	
	if (configuration.headless) {
		assert(surface == VK_NULL_HANDLE); //headless, so must not have a surface

		//make a fake swapchain:

		swapchain_extent = configuration.surface_extent;

		uint32_t requested_count = 3; //enough for FIFO-style presentation

		{ //create command pool for the headless image copy command buffers:
			assert(headless_command_pool == VK_NULL_HANDLE);
			VkCommandPoolCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = 0,
				.queueFamilyIndex = graphics_queue_family.value(),
			};
			VK( vkCreateCommandPool(device, &create_info, nullptr, &headless_command_pool) );
		}

		// create headless_swapchain
		assert(headless_swapchain.empty());

		headless_swapchain.reserve(requested_count);
		for (uint32_t i = 0; i < requested_count; ++i) {
			//add a headless "swapchain" image:
			HeadlessSwapchainImage &h = headless_swapchain.emplace_back();

			//allocate image data: (on-GPU, will be rendered to)
			h.image = helpers.create_image(
				swapchain_extent,
				surface_format.format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			);

			//allocate buffer data: (on-CPU, will be copied to)
			h.buffer = helpers.create_buffer(
				swapchain_extent.width * swapchain_extent.height * vkuFormatTexelBlockSize(surface_format.format) / vkuFormatTexelsPerBlock(surface_format.format),
				VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				Helpers::Mapped
			);

			{ //create and record copy command:
				VkCommandBufferAllocateInfo alloc_info{
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.commandPool = headless_command_pool,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1,
				};
				VK( vkAllocateCommandBuffers(device, &alloc_info, &h.copy_command) );

				//record:
				VkCommandBufferBeginInfo begin_info{
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.flags = 0, // left unspecified because command buffer will be submitted more than once
				};
				VK( vkBeginCommandBuffer(h.copy_command, &begin_info) );

				VkBufferImageCopy region{
					.bufferOffset = 0,
					.bufferRowLength = swapchain_extent.width,
					.bufferImageHeight = swapchain_extent.height,
					.imageSubresource{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = 0,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
					.imageOffset{ .x = 0, .y = 0, .z = 0 },
					.imageExtent{
						.width = swapchain_extent.width,
						.height = swapchain_extent.height,
						.depth = 1
					},
				};
				vkCmdCopyImageToBuffer(
					h.copy_command,
					h.image.handle,
					// make sure it is in this format!!
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					h.buffer.handle,
					1, &region
				);
				
				VK( vkEndCommandBuffer(h.copy_command) );
			}

			{ //create fence to signal when image is done being "presented" (copied to host memory):
				VkFenceCreateInfo create_info{
					.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
					.flags = VK_FENCE_CREATE_SIGNALED_BIT, //start signaled, because all images are available to start with
				};
				VK( vkCreateFence(device, &create_info, nullptr, &h.image_presented) );
			}
		}
		
		//copy image references into swapchain_images:
		assert(swapchain_images.empty());
		swapchain_images.assign(requested_count, VK_NULL_HANDLE);
		for (uint32_t i = 0; i < requested_count; ++i) {
			swapchain_images[i] = headless_swapchain[i].image.handle;
		}

	} else {
		assert(surface != VK_NULL_HANDLE); //not headless, so must have a surface

		//request a swapchain from the windowing system:

		//determine size, image count, and transform for swapchain:
		VkSurfaceCapabilitiesKHR capabilities;
		VK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities) );
		// current size of the surface
		swapchain_extent = capabilities.currentExtent;

		// minimum reflect the number of images that will be simultaneously in use by pres system plus one
		uint32_t requested_count = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount != 0) {
			requested_count = std::min(capabilities.maxImageCount, requested_count);
		}

		{ // make the swapchain
			VkSwapchainCreateInfoKHR create_info{
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.surface = surface,
				.minImageCount = requested_count,
				.imageFormat = surface_format.format,
				.imageColorSpace = surface_format.colorSpace, 
				.imageExtent = swapchain_extent,
				.imageArrayLayers = 1,
				.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				.preTransform = capabilities.currentTransform,
				.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = present_mode,
				.clipped = VK_TRUE,
				.oldSwapchain = VK_NULL_HANDLE // can be more eficient by passing old swapchain handle here instead of destroying
			};

			std::vector< uint32_t > queue_family_indices{
				graphics_queue_family.value(),
				present_queue_family.value()
			};

			if (queue_family_indices[0] != queue_family_indices[1]) {
				// if images will be presented on a diff queue, make sure they are shared
				create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT,
				create_info.queueFamilyIndexCount = uint32_t(queue_family_indices.size());
				create_info.pQueueFamilyIndices = queue_family_indices.data();
			}
			else {
				create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			}

			VK( vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain) );
		}

		{ // get the swapchain images
			// get array size
			uint32_t count = 0;
			VK( vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr) );
			// actually fetch contents, all owned by swapchain
			swapchain_images.resize(count);
			VK( vkGetSwapchainImagesKHR(device, swapchain, &count, swapchain_images.data()) );
		}
	}

	{ // make image views for swapchain images
		swapchain_image_views.assign(swapchain_images.size(), VK_NULL_HANDLE);
		for (size_t i = 0; i < swapchain_images.size(); ++i) {
			VkImageViewCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = swapchain_images[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = surface_format.format,
				.components{
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY,
				},
				.subresourceRange{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				},
			};

			VK( vkCreateImageView(device, &create_info, nullptr, &swapchain_image_views[i]) );
		}
	
	}
	
	{ // make image done semaphores
		VkSemaphoreCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};

		swapchain_image_dones.assign(swapchain_images.size(), VK_NULL_HANDLE);
		for (size_t i = 0; i < swapchain_image_dones.size(); ++i) {
			VK( vkCreateSemaphore(device, &create_info, nullptr, &swapchain_image_dones[i]) );
		}
	}

	if (configuration.debug) {
		std::cout << "Swapchain is now " << swapchain_images.size() << " images of size " << swapchain_extent.width << "x" << swapchain_extent.height << "." << std::endl;
	}
}


void RTG::destroy_swapchain() {
	VK( vkDeviceWaitIdle(device) ); // wait for any rendering to old swapchain to finish

	// cleanup semaphores used for waiting on the swapchain;
	for (auto &semaphore : swapchain_image_dones) {
		vkDestroySemaphore(device, semaphore, nullptr);
		semaphore = VK_NULL_HANDLE;
	}
	swapchain_image_dones.clear();

	// cleanup image views referencing the swapchain
	for (auto &image_view : swapchain_image_views) {
		vkDestroyImageView(device, image_view, nullptr);
		image_view = VK_NULL_HANDLE;
	}
	swapchain_image_views.clear();

	// forget handles to swapchain images (will destroy by deallocating the swapchain itself):
	swapchain_images.clear();
	
	if (configuration.headless) {
		//destroy the headless swapchain by first destroying each image's resources and then clearing vector:
		for (auto &h : headless_swapchain) {
			helpers.destroy_image(std::move(h.image));
			helpers.destroy_buffer(std::move(h.buffer));
			h.copy_command = VK_NULL_HANDLE; //pool deallocated below
			vkDestroyFence(device, h.image_presented, nullptr);
			h.image_presented = VK_NULL_HANDLE;
		}
		headless_swapchain.clear();

		//free all of the copy command buffers by destroying the pool from which they were allocated:
		vkDestroyCommandPool(device, headless_command_pool, nullptr);
		headless_command_pool = VK_NULL_HANDLE;
	}
	else {
		// deallocate the swapchain and (thus) its images:
		if (swapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(device, swapchain, nullptr);
			swapchain = VK_NULL_HANDLE;
		}
	}
}

static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	event.type = InputEvent::MouseMotion;
	event.motion.x = float(xpos);
	event.motion.y = float(ypos);
	event.motion.state = 0;
	for (int b = 0; b < 8 && b < GLFW_MOUSE_BUTTON_LAST; ++b) {
		if (glfwGetMouseButton(window, b) == GLFW_PRESS) {
			event.motion.state |= (1 << b);
		}
	}

	event_queue->emplace_back(event);
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	if (action == GLFW_PRESS) {
		event.type = InputEvent::MouseButtonDown;
	} else if (action == GLFW_RELEASE) {
		event.type = InputEvent::MouseButtonUp;
	} else {
		std::cerr << "Strange: unknown mouse button action." << std::endl;
		return;
	}

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	event.button.x = float(xpos);
	event.button.y = float(ypos);
	event.button.state = 0;
	for (int b = 0; b < 8 && b < GLFW_MOUSE_BUTTON_LAST; ++b) {
		if (glfwGetMouseButton(window, b) == GLFW_PRESS) {
			event.button.state |= (1 << b);
		}
	}
	event.button.button = uint8_t(button);
	event.button.mods = uint8_t(mods);

	event_queue->emplace_back(event);
}

static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	event.type = InputEvent::MouseWheel;
	event.wheel.x = float(xoffset);
	event.wheel.y = float(yoffset);

	event_queue->emplace_back(event);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	if (action == GLFW_PRESS) {
		event.type = InputEvent::KeyDown;
	} else if (action == GLFW_RELEASE) {
		event.type = InputEvent::KeyUp;
	} else if (action == GLFW_REPEAT) {
		//ignore repeats
		return;
	} else {
		std::cerr << "Strange: got unknown keyboard action." << std::endl;
	}

	event.key.key = key;
	event.key.mods = mods;

	event_queue->emplace_back(event);
}

void RTG::HeadlessSwapchainImage::save() const {
	if (save_to == "") return;

	if (image.format == VK_FORMAT_B8G8R8A8_SRGB) {
		//get a pointer to the image data copied to the buffer:
		char const *bgra = reinterpret_cast< char const * >(buffer.allocation.data());

		//convert bgra -> rgb data:
		std::vector< char > rgb(image.extent.height * image.extent.width * 3);
		for (uint32_t y = 0; y < image.extent.height; ++y) {
			for (uint32_t x = 0; x < image.extent.width; ++x) {
				rgb[(y * image.extent.width + x) * 3 + 0] = bgra[(y * image.extent.width + x) * 4 + 2];
				rgb[(y * image.extent.width + x) * 3 + 1] = bgra[(y * image.extent.width + x) * 4 + 1];
				rgb[(y * image.extent.width + x) * 3 + 2] = bgra[(y * image.extent.width + x) * 4 + 0];
			}
		}

		//write ppm file:
		std::ofstream ppm(save_to, std::ios::binary);
		ppm << "P6\n"; //magic number + newline
		ppm << image.extent.width << " " << image.extent.height << "\n"; //image size + newline
		ppm << "255\n"; //max color value + newline
		ppm.write(rgb.data(), rgb.size()); //rgb data in row-major order, starting from the top left

	} else {
		std::cerr << "WARNING: saving format " << string_VkFormat(image.format) << " not supported." << std::endl;
	}
}

void RTG::run(Application &application) {
	// initial on_swapchain
	auto on_swapchain = [&,this]() {
		application.on_swapchain(*this, SwapchainEvent{
			.extent = swapchain_extent,
			.images = swapchain_images,
			.image_views = swapchain_image_views,
		});
	};
	on_swapchain();

	float headless_dt = 0.0f;
	std::string headless_save = "";

	// set up event handling
	std::vector< InputEvent > event_queue;
	if (!configuration.headless) {
		glfwSetWindowUserPointer(window, &event_queue);
		//glfwSetWindowOpacity(window, 0.5f);

		glfwSetCursorPosCallback(window, cursor_pos_callback);
		glfwSetMouseButtonCallback(window, mouse_button_callback);
		glfwSetScrollCallback(window, scroll_callback);
		glfwSetKeyCallback(window, key_callback);
	}

	uint32_t headless_next_image = 0;

	// initial time 
	std::chrono::high_resolution_clock::time_point before = std::chrono::high_resolution_clock::now();

	while (configuration.headless || !glfwWindowShouldClose(window)) {
		//event handling:
		if (configuration.headless) {
			// read events from stdin:
			std::string line;
			while (std::getline(std::cin, line)) {
				try {
					std::istringstream iss(line);
					iss.imbue(std::locale::classic()); //ensure floating point numbers always parse with '.' as the separator

					// read type
					std::string type;
					if (!(iss >> type)) throw std::runtime_error("failed to read event type");

					//TODO: type-specific parsing
					if (type == "AVAILABLE") {  //AVAILABLE dt [save.ppm]
						// read dt
						if (!(iss >> headless_dt)) throw std::runtime_error("failed to read dt");
						if (headless_dt < 0.0f) throw std::runtime_error("dt less than zero");

						// check for save file name
						if (iss >> headless_save) {
							if (!headless_save.ends_with(".ppm")) throw std::runtime_error("output filename ("" + headless_save + "") must end with .ppm");
						}	

						// check for trailing junk
						char junk;
						if (iss >> junk) throw std::runtime_error("trailing junk in event line");

						// stop parsing events so a frame can draw
						break;

					} else {
						throw std::runtime_error("unrecognized type");
					}

					//TODO: type-specific parsing

				} 
				catch (std::exception &e) {
					std::cerr << "WARNING: failed to parse event (" << e.what() << ") from: "" << line << ""; ignoring it." << std::endl;
				}
			}
			// if we've run out of events, stop running main loop:
			if (!std::cin) break;
		}
		else {
			glfwPollEvents();
		}
		//deliver all input events to application:
		for (InputEvent const &input : event_queue) {
			application.on_input(input);
		}
		event_queue.clear();

		{ // elapsed time handling:
			std::chrono::high_resolution_clock::time_point after = std::chrono::high_resolution_clock::now();
			float dt = float(std::chrono::duration< double >(after - before).count());
			before = after;

			dt = std::min(dt, 0.1f); // lag if framerate dips too low
			
			// in headless mode, override dt:
			if (configuration.headless) dt = headless_dt;

			application.update(dt);
		}

		uint32_t workspace_index;
		{ //acquire a workspace:
			assert(next_workspace < workspaces.size());
			workspace_index = next_workspace;
			next_workspace = (next_workspace + 1) % workspaces.size();

			//wait until the workspace is not being used:
			VK( vkWaitForFences(device, 1, &workspaces[workspace_index].workspace_available, VK_TRUE, UINT64_MAX) );

			//mark the workspace as in use:
			VK( vkResetFences(device, 1, &workspaces[workspace_index].workspace_available) );
		}

		uint32_t image_index = -1U;
		if (configuration.headless) {
			assert(swapchain == VK_NULL_HANDLE);

			//acquire the least-recently-used headless swapchain image:
			assert(headless_next_image < uint32_t(headless_swapchain.size()));
			image_index = headless_next_image;
			headless_next_image = (headless_next_image + 1) % uint32_t(headless_swapchain.size());

			//wait for image to be done copying to buffer
			VK( vkWaitForFences(device, 1, &headless_swapchain[image_index].image_presented, VK_TRUE, UINT64_MAX) );

			//save buffer, if needed:
			if (headless_swapchain[image_index].save_to != "") {
				headless_swapchain[image_index].save();
				headless_swapchain[image_index].save_to = "";
			}

			//remember if next frame should be saved:
			headless_swapchain[image_index].save_to = headless_save;

			//TODO: mark next copy as pending
			VK( vkResetFences(device, 1, &headless_swapchain[image_index].image_presented) );

			//signal GPU that image is "available for rendering to" NEEDED bc we are doing both headless and swapchained mode, technically fences are enough for a swapchain only app
			VkSubmitInfo submit_info{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &workspaces[workspace_index].image_available // semaphore 
			};
			VK( vkQueueSubmit(graphics_queue, 1, &submit_info, nullptr) );
		}
		else {
			//acquire a swapchain image:
			retry:
			//Ask the swapchain for the next image index -- note careful return handling:
			if (VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, workspaces[workspace_index].image_available, VK_NULL_HANDLE, &image_index);
				result == VK_ERROR_OUT_OF_DATE_KHR) {
				//if the swapchain is out-of-date, recreate it and run the loop again:
				std::cerr << "Recreating swapchain because vkAcquireNextImageKHR returned " << string_VkResult(result) << "." << std::endl;
				
				recreate_swapchain();
				on_swapchain();

				goto retry;
			} else if (result == VK_SUBOPTIMAL_KHR) {
				//if the swapchain is suboptimal, render to it and recreate it later:
				std::cerr << "Suboptimal swapchain format -- ignoring for the moment." << std::endl;
			} else if (result != VK_SUCCESS) {
				//other non-success results are genuine errors:
				throw std::runtime_error("Failed to acquire swapchain image (" + std::string(string_VkResult(result)) + ")!");
			}
		}
		// call render function: 
		application.render(*this, RenderParams{
			.workspace_index = workspace_index, 
			.image_index = image_index,
			.image_available = workspaces[workspace_index].image_available,
			.image_done = swapchain_image_dones[image_index],
			.workspace_available = workspaces[workspace_index].workspace_available,
		});


		if (configuration.headless) {
			// in headless mode, submit the copy command we recorded previously:

			// will wait in the transfer stage for the image_done to be signaled:
			VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			VkSubmitInfo submit_info{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &swapchain_image_dones[image_index],
				.pWaitDstStageMask = &wait_stage,
				.commandBufferCount = 1,
				.pCommandBuffers = &headless_swapchain[image_index].copy_command,
			};
			// handles GPU synchronization by pausing exec in transfer srtage until image done semaphore is signalled
			// runs copy commnd with pCommandBuffer
			// CPU sync with image_presented fence
			VK( vkQueueSubmit(graphics_queue, 1, &submit_info, headless_swapchain[image_index].image_presented) );
		}
		else { //queue the work for presentation:
			VkPresentInfoKHR present_info{
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &swapchain_image_dones[image_index],
				.swapchainCount = 1,
				.pSwapchains = &swapchain,
				.pImageIndices = &image_index,
			};

			assert(present_queue);

			//note, again, the careful return handling:
			if (VkResult result = vkQueuePresentKHR(present_queue, &present_info);
			    result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				std::cerr << "Recreating swapchain because vkQueuePresentKHR returned " << string_VkResult(result) << "." << std::endl;
				recreate_swapchain();
				on_swapchain();
			} else if (result != VK_SUCCESS) {
				throw std::runtime_error("failed to queue presentation of image (" + std::string(string_VkResult(result)) + ")!");
			}
		}

		//TODO: present image (resize swapchain if needed)
	}

	// wait for any in-flight "headless" frames marked fo saving to finish:
	if (configuration.headless) {
		for (size_t i = 0; i < headless_swapchain.size(); ++i) {
			uint32_t image_index = headless_next_image;
			headless_next_image = (headless_next_image + 1) % uint32_t(headless_swapchain.size());

			// block until the image is finished being "presented" (copied to host);
			VK( vkWaitForFences(device, 1, &headless_swapchain[image_index].image_presented, VK_TRUE, UINT64_MAX) );

			// save if requested:
			if (headless_swapchain[image_index].save_to != "") {
				headless_swapchain[image_index].save();
				headless_swapchain[image_index].save_to = "";
			}
		}
	}

	//tear down event handling:
	if (!configuration.headless) {
		glfwSetMouseButtonCallback(window, nullptr);
		glfwSetCursorPosCallback(window, nullptr);
		glfwSetScrollCallback(window, nullptr);
		glfwSetKeyCallback(window, nullptr);

		glfwSetWindowUserPointer(window, nullptr);
	}


}
