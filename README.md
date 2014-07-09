reed-framework
==============

Basic framework for D3D11 init, model/texture loading, camera movement, etc.

Instructions:
* `#include <framework.h>`
* Link with `framework.lib`, or just add `framework.vcxproj` to your solution and reference it
* Also link with `d3d11.lib`, `dxguid.lib`, and `d3dx11.lib`

Current features:
* COM smart pointer—handles COM reference counting while being mostly transparent
* D3D11 window class—handles window creation, D3D11 init, message loop, resizing, etc.
* Functions for blitting textures
* Function for drawing a full-screen triangle
* Common D3D11 state objects—rasterizer, depth/stencil, blend, sampler
* D3D11 constant buffer class
* D3D11 texture classes, with image loading via D3DX
* D3D11 render target class
* D3D11 mesh class, with .obj file loading
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
* Precompilation tools for scenes? (Cutting load times)
* Async asset loading
* Better input system; gamepad support
* Better image loading than D3DX
* Screenshotting—both LDR and HDR
* Multi-monitor and multi-GPU awareness
* GPU clock speed monitoring à la GPU-Z
* Rewrite to add OpenGL & cross-platform support
