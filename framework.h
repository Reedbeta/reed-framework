#pragma once

#include <util.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
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
		{ \
			HRESULT hr = f; \
			CHECK_ERR_MSG(SUCCEEDED(hr), "D3D call failed with error code: 0x%08x\nFailed call: %s", hr, #f); \
		}

#define CHECK_D3D_WARN(f) \
		{ \
			HRESULT hr = f; \
			CHECK_WARN_MSG(SUCCEEDED(hr), "D3D call failed with error code: 0x%08x\nFailed call: %s", hr, #f); \
		}

#include "comptr.h"

#include "camera.h"
#include "cbuffer.h"
#include "d3d11-window.h"
#include "gpuprofiler.h"
#include "material.h"
#include "mesh.h"
#include "rendertarget.h"
#include "shadow.h"
#include "texture.h"
#include "timer.h"

#include "asset.h"
