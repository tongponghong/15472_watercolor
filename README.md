# Real-Time Watercolor Stylization and 3D Drawing

A Vulkan renderer that paints 3D scenes in a watercolor style in real time, and an in-progress drawing tool that lets you draw strokes directly into those scenes.

This repository holds three layers of work, each built on the one before it:

1. **The Vulkan engine.** A scene viewer written from scratch for 15-472 (Real-Time Graphics) at CMU: `.s72` scene-graph loading, materials, three light types, shadow maps, tone mapping, and frustum culling.
2. **Real-time watercolor stylization.** The 15-472 final project, by Ollie Arrison and Tunger Hong, adding a watercolor reflectance model in the object shader plus three compute passes (Gaussian blur, 4D joint-bilateral bleed, and final stylization with a paper texture).
3. **The 3D drawing app.** Current work in progress: an interactive mode that projects mouse strokes onto a depth plane in the scene, so hand-drawn marks live in world space and can be run through the watercolor pipeline alongside the rest of the geometry.

<!-- Everything runs in one executable. The stylization is always on; the drawing mode is toggled at runtime. -->

The stylization follows the paper [Art-directed watercolor stylization of 3D animations in real-time](https://doi.org/10.1016/j.cag.2017.05.008) (Montesdeoca et al.), with a few deliberate departures noted below.

---

## Table of contents

- [Building](#building)
- [Running](#running)
- [Command-line arguments](#command-line-arguments)
- [Controls](#controls)
- [Project 1: the Vulkan engine](#project-1-the-vulkan-engine)
- [Project 2: watercolor stylization](#project-2-watercolor-stylization)
- [Project 3: the 3D drawing app](#project-3-the-3d-drawing-app-in-progress)
- [Render graph](#render-graph)
- [Performance](#performance)
- [Repository layout](#repository-layout)
- [Status and known limitations](#status-and-known-limitations)
- [Credits](#credits)

---

## Building

Requirements:

- A C++20 compiler (clang on macOS, g++ on Linux, MSVC on Windows)
- The [Vulkan SDK](https://vulkan.lunarg.com/), including `glslc` for shader compilation
- [GLFW 3.4](https://www.glfw.org/)
- [Node.js](https://nodejs.org/), only to run the build script

[Dear ImGui](https://github.com/ocornut/imgui) is vendored as a submodule, so clone recursively:

```sh
git clone --recurse-submodules <repo-url>
# or, in an existing clone:
git submodule update --init --recursive
```

The build is driven by `Maekfile.js`, a self-contained Node build system. From `code_watercolor/`:

```sh
node Maekfile.js
```

Useful flags: `-jN` to limit parallel jobs, `-v` for verbose output, `-q` to quit on the first error.

This produces two executables:

| Target | Description |
| --- | --- |
| `bin/main` | The scene viewer, stylizer, and drawing app |
| `bin/cube` | A utility that precomputes Lambertian irradiance lookup maps from an RGBE cube map |

Shaders in `shaders/` are compiled to SPIR-V and emitted as C headers under `spv/`, which the pipeline sources in `pipelines/` include directly, so editing a shader rebuilds only the pipeline that uses it. Build state is cached by content hash in `maek-cache.json`; delete it to force a full rebuild.

If the SDK or GLFW are not at the default locations, override them:

```sh
export VULKAN_SDK=/path/to/VulkanSDK/<version>/<platform>
export GLFW_DIR=/path/to/glfw-3.4/out   # Linux only
```

---

## Running

The stylization needs a paper texture, which the final compute pass layers over the result:

```sh
cd code_watercolor
./bin/main --scene ../report/scenes/<scene>.s72 --paper ../report/custom_scenes/Rough5k.png --exposure 0.0
```

Paper textures are in `report/custom_scenes/` (a laid paper and a rough paper, each with a normal map).

---

## Command-line arguments

| Argument | Required | Description |
| --- | --- | --- |
| `--scene scene.s72` | Required | Load the scene from `scene.s72`. |
| `--paper path/to/paper.png` | Required for stylization | Paper texture composited over the final image in `style.comp`. |
| `--camera name` | Optional | Start looking through the named scene camera. |
| `--physical-device name` | Optional | Use the physical device whose `deviceName` matches. Guesses otherwise. |
| `--drawing-size w h` | Optional | Starting width and height of the drawable surface. Defaults to 800x540. |
| `--headless` | Optional | Run without a window, reading an event script from standard input. |
| `--lambertian path/to/map.png` | Optional | Precomputed Lambertian lookup map for diffuse environment lighting. |
| `--exposure float` | Optional | Exposure applied to all materials. Defaults to `0.0`. |
| `--tone-map <linear\|reinhard>` | Optional | Tone-mapping operator. Defaults to `linear`. |
| `--debug`, `--no-debug` | Optional | Turn Vulkan validation layers and extra debug output on or off. |
| `--culling` | Optional | Accepted for compatibility. Frustum culling is always on; the flag is a no-op. |

### Cube utility

```sh
./bin/cube input_cubemap.png --lambertian out.png
```

Writes the diffuse irradiance cube map as RGBE8. Output resolution is set by constants in `make_cubemap.cpp`; sample outputs at 4x4 through 32x32 are in `report/`.

---

## Controls

### Cameras

| Input | Action |
| --- | --- |
| `1` | Scene camera mode (cameras authored in the `.s72` file). |
| `2` | Free orbit camera. |
| `0` | Debug camera, which views the active scene camera's frustum from outside. |
| Left / Right arrow | Cycle through the scene's cameras (scene camera mode only). |
| Left mouse drag | Tumble (free camera). |
| Shift + left mouse drag | Pan (free camera). |
| Scroll wheel | Dolly in and out (free camera). |

### Drawing

| Input | Action |
| --- | --- |
| `D` | Toggle drawing mode on and off. |
| Left mouse drag | Draw a stroke, while drawing mode is on. |
| Up arrow | Push the drawing plane farther from the camera (by 0.5). |
| Down arrow | Pull the drawing plane closer to the camera (by 0.5). |

### Shading

| Input | Action |
| --- | --- |
| `]` | Increase exposure by 0.2. |
| `[` | Decrease exposure by 0.2. |

An ImGui overlay reports frame time, FPS, the number of objects surviving culling, camera and target positions, drawn-vertex count, and the current stroke indices.

---

## Project 1: the Vulkan engine

The base renderer parses `.s72` scenes (JSON, with mesh attributes streamed from binary `.b72` buffers), flattens the scene graph with an explicit stack, and packs every mesh into one static vertex buffer so instances need only a first-vertex offset and a count.

Supported: authored, free, and debug cameras with aspect-correct letterboxing and pillarboxing; animation drivers with linear interpolation for translation and scale and spherical interpolation for rotation; Lambertian, environment, and mirror materials with tangent-space normal maps; sun, sphere, and spot lights, each in its own storage buffer with its own shader contribution function; shadow maps rendered into a 4096x4096 atlas; linear and Reinhard tone mapping.

Frustum culling runs every frame in `helperlibs/culling.hpp`, converting each object's axis-aligned bounding box into a view-space oriented bounding box and testing it against the frustum, following [Bruce Robinson's improved frustum culling writeup](https://bruop.github.io/improved_frustum_culling/). Only surviving instances reach the transforms buffer and the draw loop.

<!-- VIDEO: base renderer walkthrough, camera modes and culling. Drag a video file here in the GitHub editor. -->

The earlier assignment-by-assignment version of this engine lives in a separate repository; this one is the fork the final project was built on.

### Watercolor reflectance model

Rather than shading physically, the object fragment shader models how a painter dilutes pigment with water. A dilution area is accumulated across all lights, standing in for the region the light touches, and drives how far the base color washes toward the paper color.

The cangiante effect, a non-realistic hue shift from dark to light, is where this implementation departs from the paper. The paper adds a cangiante color weighted by intensity, which brightens the result. Because real watercolor gets its shifts by layering translucent washes, which darkens rather than brightens, we instead composite with alpha-over: the cangiante color as background at the cangiante strength, the base color as foreground at the dilution area. The result is more subtle and holds its values better.

<!-- VIDEO: reflectance model, dilution and cangiante. Drag a video file here. -->
<!-- Stills: report/Images/A.png and report/Images/B.png compare the paper's version against ours -->

---

## Project 2: watercolor stylization

Three compute passes turn the shaded image into a painting. The object pass writes out three images: color, a control image of fractal Brownian motion noise generated from world-space vertex positions, and linearized depth.

### Gaussian blur (`ComputePipeline`, `gaussian.comp`)

- A small fixed-kernel blur over the color image, two bindings in and out. Kernel weights are generated ahead of time by `computeKernel.py`.

<!-- VIDEO: Gaussian blur pass. Drag a video file here. -->

### Pigment bleed (`BleedPipeline`, `bilateral.comp`)

- A 4D joint-bilateral filter that reproduces how pigment bleeds across a wet page, taking color, control, and depth as inputs. Near and far clipping planes arrive as push constants so depth can be linearized in the shader; the filter then blurs across pixels that sit sufficiently far behind the current one, letting foreground color bleed outward without background color leaking in. We additionally suppress blurring across large depth discontinuities, which preserves silhouettes that the paper's formulation washes out.

<!-- VIDEO: bleed pass and depth-aware blurring. Drag a video file here. -->

### Final stylization (`StylePipeline`, `style.comp`)

- There are in total six bindings: five inputs and one output. The control image decides, per pixel, how much of the blurred image versus the bled image to take, sharpening some regions while softening others, and the paper texture is layered over the result before the image is copied to the swapchain.

<!-- VIDEO: full stylized animation. Drag a video file here. -->
<!-- Local source: report/watercolor.mp4 -->

---

## Project 3: the 3D drawing app (in progress)

- **Goal:**

Draw into a 3D scene the way you would sketch on paper, and have those strokes stylized as watercolor along with everything else.

- **How it works now:**

With drawing mode on, each mouse-motion event is clamped to the viewport, converted to normalized device coordinates, and turned into a ray by unprojecting a near point and a far point (`near_clip_pts` and `far_clip_pts`). `point_to_world` intersects that ray with a camera-facing plane whose distance is set by `user_draw_depth_scale`, so up and down arrows move the paper toward or away from you. Strokes accumulate in `user_drawn_points_WORLD` in world space, with `drawn_strokes_indices` recording where each stroke ends so separate strokes do not connect. Because the points are in world space, the drawing holds its position in the scene as the camera orbits.

- **Working:** 

Stroke capture and world-space projection, correct mapping under letterboxing and pillarboxing, Retina and high-DPI scaling (window content scale is applied to cursor positions and the scissor rectangle), stroke separation on mouse release, and the ImGui panel for inspecting drawing state.

- **In progress:** 

Replacing polyline rendering with stamped brush quads. `DrawPipeline` is written with its own transform and brush-texture descriptor sets, but `shaders/stamps.vert` and `shaders/stamps.frag` are still empty and the pipeline is not yet in the build, so strokes currently render through the lines pipeline.

<!-- VIDEO: drawing in 3D, strokes holding position as the camera orbits. Drag a video file here. -->

<!-- VIDEO: moving the drawing plane with the up and down arrows. Drag a video file here. -->

<!-- VIDEO: drawn strokes running through the watercolor stylization. Drag a video file here. -->

---

## Render graph

One frame, in order:

```
shadow pass          depth from each light into the shadow atlas
object pass          color image, control image (FBM noise), depth image
  barrier 1          blur and bleed outputs moved to VK_IMAGE_LAYOUT_GENERAL
gaussian.comp        color -> blurred
bilateral.comp       color + control + depth -> bled
  barrier 2          blurred and bled switch from write to read; final output to GENERAL
style.comp           color + control + blurred + bled + paper -> final
  barrier 3          final to TRANSFER_SRC_OPTIMAL, swapchain to TRANSFER_DST_OPTIMAL
copy                 final image copied to the swapchain image
ImGui pass           overlay
```

A diagram of this is in `report/Images/pipeline.png`, and the write-up in `report/final-report.html` walks through each barrier.

---

## Performance

Frame time was measured against filter kernel size, holding one filter fixed while sweeping the other. Both filters cost roughly the same per unit of kernel size, but the Gaussian blur starts to hurt at noticeably smaller kernels than the bilateral filter, and both stay comfortably real-time across the tested range. Raw timings are in `report/gaussian_*.txt` and `report/bleed_*.txt`.

The obvious next optimization is making the Gaussian blur two-pass like the bilateral filter, and computing kernel weights on the CPU into a buffer so the radius can be changed at runtime instead of being compiled in.

<!-- VIDEO: kernel size sweep. Drag a video file here. -->

---

## Repository layout

```
code_watercolor/
  main.cpp                    entry point
  RTG.cpp / RTG.hpp           Vulkan instance, device, swapchain, main loop, argument parsing
  Tutorial.cpp / Tutorial.hpp the application: scene graph, cameras, lights, drawing mode, frame recording
  pipelines/
    Tutorial-BackgroundPipeline.cpp   procedural background
    Tutorial-LinesPipeline.cpp        debug lines, bounding boxes, and current stroke rendering
    Tutorial-ObjectsPipeline.cpp      shaded geometry and the watercolor reflectance model
    ShadowPipeline.cpp                shadow atlas depth pass
    ComputePipeline.cpp               Gaussian blur
    BleedPipeline.cpp                 4D joint-bilateral bleed
    StylePipeline.cpp                 final stylization and paper compositing
    DisplayPipeline.cpp               presentation
    DrawPipeline.cpp                  brush stamps (not yet in the build)
  shaders/                    GLSL sources, compiled to SPIR-V by the build
  VertexTypes/                vertex formats and their Vulkan descriptions
  helperlibs/
    S72_loader/               .s72 and .b72 scene parsing
    culling.hpp               oriented bounding box frustum culling
    sceneGrapher/             scene graph structures
    sejp_lib/                 JSON parsing
    mathlibs/                 vec3, mat4, sampling helpers
    stb_image_lib/            image loading and writing
    rgb_decoders.hpp          RGBE and RGB9E5 conversion
    imgui/                    Dear ImGui (submodule)
  computeKernel.py            generates Gaussian kernel weights
  cube.cpp, make_cubemap.cpp  the Lambertian precomputation utility
  Maekfile.js                 the build system

report/
  final-report.html           the final project write-up
  Images/                     figures, including the pipeline diagram
  scenes/                     test scenes, including the light stress scenes
  custom_scenes/              paper textures and their normal maps
  watercolor.mp4              stylized animation capture
  gaussian_*.txt, bleed_*.txt frame time measurements
```

---

## Status and known limitations

- Brush stamping is unfinished. `shaders/stamps.vert` and `shaders/stamps.frag` are empty and `DrawPipeline.cpp` is not yet compiled by `Maekfile.js`; strokes render as polylines through the lines pipeline in the meantime.
- Drawn strokes are not yet fed through the stylization passes as first-class geometry.
- The Gaussian blur is single-pass with compile-time kernel constants, so the radius cannot be changed at runtime.
- The PBR material path in `shaders/objects.frag` is stubbed out. Only Lambertian, environment, and mirror materials are shaded.
- Direct lighting is applied to Lambertian materials only.
<!-- - Two ImGui submodule entries are listed in `.gitmodules`, one of which is stale. -->
- Development is split across `main` and a `compute` branch; check which one you are on before building.

---

## Credits

- Watercolor stylization by Ollie Arrison and Tunger Hong, as the 15-472 final project. The base engine and the drawing app are by Tunger Hong.
- Built on the nakluV Vulkan tutorial framework from 15-472.
- Stylization technique from [Art-directed watercolor stylization of 3D animations in real-time](https://doi.org/10.1016/j.cag.2017.05.008), with the cangiante and depth-discontinuity departures described above.
- Frustum culling adapted from [https://bruop.github.io/improved_frustum_culling/](https://bruop.github.io/improved_frustum_culling/).
- RGBE encoding and decoding adapted from the 15-466 image-based lighting code ([https://github.com/ixchow/15-466-ibl](https://github.com/ixchow/15-466-ibl)) and the OpenGL specification for RGB9E5.
- Tone mapping reference: [https://64.github.io/tonemapping/](https://64.github.io/tonemapping/).
- Dear ImGui by Omar Cornut; `stb_image` by Sean Barrett.

Demo scene assets:

- Statue: [https://sketchfab.com/3d-models/estatua-statue-907ae2bb4f23423db76b7ec9cfe6b0e9](https://sketchfab.com/3d-models/estatua-statue-907ae2bb4f23423db76b7ec9cfe6b0e9)
- Mountain terrain: [https://sketchfab.com/3d-models/snowy-mountain-terrain-9fa3c56fd32746bcb0e06cd2c4229ca0](https://sketchfab.com/3d-models/snowy-mountain-terrain-9fa3c56fd32746bcb0e06cd2c4229ca0)
- Castle: [https://sketchfab.com/3d-models/hrusov-castle-993f6d456a434490b7ad3b9456b97a19](https://sketchfab.com/3d-models/hrusov-castle-993f6d456a434490b7ad3b9456b97a19)
- Snow texture: [https://polyhaven.com/a/snow_field_aerial](https://polyhaven.com/a/snow_field_aerial)
- Plant models: [https://polyhaven.com/models/nature](https://polyhaven.com/models/nature)
- Sky texture: [https://freestylized.com/all-skybox/](https://freestylized.com/all-skybox/)
