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
		ID3D11Device * pDevice,
		const char * path,
		int flags = LOADTEX_Mipmap | LOADTEX_SRGB);

	ID3D11ShaderResourceView * Create1x1Texture(
		ID3D11Device * pDevice,
		float r, float g, float b,
		bool linear = false);

	ID3D11ShaderResourceView * CreateTextureFromMemory(
		ID3D11Device * pDevice,
		uint width, uint height,
		DXGI_FORMAT format,
		const void * pPixels);

	const char * NameOfFormat(DXGI_FORMAT format);
	uint BitsPerPixel(DXGI_FORMAT format);
}
