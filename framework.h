#pragma once

#include <util-all.h>

#include <string>
#include <unordered_map>
#include <vector>

#define NOMINMAX
#include <windows.h>
#include <d3d11.h>

namespace Framework
{
	using namespace util;
	using util::byte;		// Needed because Windows also defines the "byte" type

	class AssetPack;
}

#define CHECK_D3D(f) \
			CHECK_ERR(SUCCEEDED(f))

#define CHECK_D3D_WARN(f) \
			CHECK_WARN(SUCCEEDED(f))

#include "comptr.h"

#include "camera.h"
#include "cbuffer.h"
#include "d3d11-window.h"
#include "gpuprofiler.h"
#include "mesh.h"
#include "rendertarget.h"
#include "texture.h"
#include "timer.h"

#include "asset.h"
