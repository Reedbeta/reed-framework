reed-framework
==============

Basic framework for D3D11 init, model/texture loading, camera movement, etc.

Instructions:
* `#include <framework.h>`
* Link with `framework.lib`, or just add `framework.vcxproj` to your solution and reference it
* Also link with `d3d11.lib`, `dxgi.lib`, and `xinput_9_1_0.lib` (or the newer `xinput1_4.dll`)

Current features:
* Asset compilation system for pre-processing graphics data into an engine-friendly format
  * Compiles meshes from .obj format; also parses .mtl materials
  * Compiles textures from any format stb_image supports, resampling to power-of-two size and generating mipmaps
  * Stores compiled data in an asset pack in .zip format for easy distribution
  * Identifies out-of-date assets by timestamp or file format version number, and recompiles only out-of-date or missing ones
* COM smart pointer—handles COM reference counting while being mostly transparent
* D3D11 window class—handles window creation, D3D11 init, message loop, resizing, etc.
* Functions for blitting textures
* Function for drawing a full-screen triangle
* Common D3D11 state objects—rasterizer, depth/stencil, blend, sampler
* D3D11 constant buffer class
* D3D11 texture classes: 2D, cubemap, 3D
* D3D11 render target class
* D3D11 mesh class
* Texture and material library classes: map string names to textures/materials stored in an asset pack
* Mipmap size calculations
* Camera classes—FPS-style and Maya-style, and object hierarchy for adding more
* CPU timer—smooths timestep for stability; also tracks total time since startup
* GPU profiler—manages queries, buffers a few frames, and smooths the results

Todo list (in no particular order):
* Debug line rendering
* AntTweakBar integration / extensions
* Console for displaying realtime errors/warnings without stopping the world
* Video memory usage prediction/tracking
* Shader compilation framework
* Scene rendering framework, supporting multiple objects/materials, etc.
* Postprocessing framework
* Multicore asset compilation
* Async asset loading
* Better input system; gamepad support
* Screenshotting—both LDR and HDR
* Multi-monitor and multi-GPU awareness
* GPU clock speed monitoring à la GPU-Z
* OpenGL & cross-platform support
