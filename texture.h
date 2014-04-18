#pragma once

namespace Framework
{
	// Texture loading

	enum LOADTEX_FLAGS
	{
		LOADTEX_Mipmap		= 0x1,		// Load mipmaps, or generate if not present in file
		LOADTEX_SRGB		= 0x2,		// Interpret image data as SRGB
		LOADTEX_HDR			= 0x4,		// Store image in float16 format
		LOADTEX_Cubemap		= 0x8,		// Image is expected to be a cubemap
	};

	ID3D11ShaderResourceView * LoadTexture(
			const char * path,
			ID3D11Device * pDevice,
			int flags = LOADTEX_Mipmap | LOADTEX_SRGB);

	ID3D11ShaderResourceView * Create1x1Texture(
			float r, float g, float b,
			ID3D11Device * pDevice,
			bool linear = false);
}
