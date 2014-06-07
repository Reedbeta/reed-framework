#pragma once

#include <util-all.h>
#include <vector>
#include <unordered_map>

#define NOMINMAX
#include <windows.h>
#include <d3d11.h>

namespace Framework
{
	using namespace util;
	using util::byte;		// Needed because Windows also defines the "byte" type
}

#define CHECK_D3D(f) \
			CHECK_ERR(SUCCEEDED(f))

#define CHECK_D3D_WARN(f) \
			CHECK_WARN(SUCCEEDED(f))

#include "comptr.h"

#include "camera.h"
#include "cbuffer.h"
#include "d3d11-window.h"
#include "mesh.h"
#include "rendertarget.h"
#include "texture.h"
#include "timer.h"