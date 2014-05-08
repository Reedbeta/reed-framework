#include "framework.h"
#include <d3dx11.h>

namespace Framework
{
	using namespace util;
	using util::byte;				// Needed because Windows also defines the "byte" type

	ID3D11ShaderResourceView * LoadTexture(
		ID3D11Device * pDevice,
		const char * path,
		int flags /*= LOADTEX_Mipmap | LOADTEX_SRGB*/)
	{
		bool mipmap = (flags & LOADTEX_Mipmap) != 0;
		bool SRGB = (flags & LOADTEX_SRGB) != 0;
		bool HDR = (flags & LOADTEX_HDR) != 0;
		bool cubemap = (flags & LOADTEX_Cubemap) != 0;

		// HDR bitmaps are always in linear color space
		if (HDR)
			ASSERT_WARN_MSG(!SRGB, "HDR bitmaps cannot be in SRGB space");

		// Load the texture, generating mipmaps if requested

		D3DX11_IMAGE_LOAD_INFO imgLoadInfo;
		imgLoadInfo.MipLevels = mipmap ? D3DX11_DEFAULT : 1;
		imgLoadInfo.Usage = D3D11_USAGE_IMMUTABLE;
		imgLoadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		imgLoadInfo.MiscFlags = cubemap ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
		imgLoadInfo.Format = HDR ? DXGI_FORMAT_R16G16B16A16_FLOAT :
								(SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM);
		imgLoadInfo.Filter = D3DX11_FILTER_TRIANGLE | (SRGB ? D3DX11_FILTER_SRGB : 0);
		imgLoadInfo.MipFilter = D3DX11_FILTER_TRIANGLE;

		ID3D11ShaderResourceView * pSrv = nullptr;
		CHECK_D3D(D3DX11CreateShaderResourceViewFromFile(
								pDevice,
								path,
								&imgLoadInfo,
								nullptr,		// no thread pump
								&pSrv,
								nullptr));		// no async return value

#if ENABLE_LOGGING
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		pSrv->GetDesc(&srvDesc);

		UINT cMipLevels = 1;
		const char * strDimension = "other";
		switch (srvDesc.ViewDimension)
		{
		case D3D11_SRV_DIMENSION_TEXTURE2D:
			strDimension = "2D";
			cMipLevels = srvDesc.Texture2D.MipLevels;
			break;

		case D3D11_SRV_DIMENSION_TEXTURECUBE:
			strDimension = "cube";
			cMipLevels = srvDesc.TextureCube.MipLevels;
			break;
	
		default:
			WARN("Unexpected SRV dimension %d", srvDesc.ViewDimension);
			break;
		}

		LOG(
			"Loaded %s - %s, format %s, %d mip levels",
			path, strDimension, NameOfFormat(srvDesc.Format), cMipLevels);
#endif // ENABLE_LOGGING

		return pSrv;
	}

	ID3D11ShaderResourceView * Create1x1Texture(
		ID3D11Device * pDevice,
		float r, float g, float b,
		bool linear /*= false*/)
	{
		// Convert floats to 8-bit format
		byte4 rgba = makebyte4(
						makebyte3(round(255.0f * saturate(makefloat3(r, g, b)))),
						255);

		D3D11_TEXTURE2D_DESC texDesc = 
		{
			1, 1, 1, 1,
			linear ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			{ 1, 0 },
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_SHADER_RESOURCE,
			0, 0,
		};

		D3D11_SUBRESOURCE_DATA initialData = { rgba, sizeof(byte4), };
		comptr<ID3D11Texture2D> pTex;
		CHECK_D3D(pDevice->CreateTexture2D(&texDesc, &initialData, &pTex));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc =
		{
			texDesc.Format,
			D3D11_SRV_DIMENSION_TEXTURE2D,
		};
		srvDesc.Texture2D.MipLevels = 1;

		ID3D11ShaderResourceView * pSrv = nullptr;
		CHECK_D3D(pDevice->CreateShaderResourceView(pTex, &srvDesc, &pSrv));

		return pSrv;
	}

	ID3D11ShaderResourceView * CreateTextureFromMemory(
		ID3D11Device * pDevice,
		uint width, uint height,
		DXGI_FORMAT format,
		const void * pPixels)
	{
		D3D11_TEXTURE2D_DESC texDesc =
		{
			width, height, 1, 1,
			format,
			{ 1, 0 },
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_SHADER_RESOURCE,
			0, 0,
		};

		D3D11_SUBRESOURCE_DATA initialData = { pPixels, width * BytesPerPixel(format) };
		comptr<ID3D11Texture2D> pTex;
		CHECK_D3D(pDevice->CreateTexture2D(&texDesc, &initialData, &pTex));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc =
		{
			format,
			D3D11_SRV_DIMENSION_TEXTURE2D,
		};
		srvDesc.Texture2D.MipLevels = 1;

		ID3D11ShaderResourceView * pSrv = nullptr;
		CHECK_D3D(pDevice->CreateShaderResourceView(pTex, &srvDesc, &pSrv));

		return pSrv;
	}



	const char * NameOfFormat(DXGI_FORMAT format)
	{
		// !!!UNDONE: handle *all* DXGI formats here

		switch (format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM:		return "R8G8B8A8_UNORM";
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:	return "R8G8B8A8_UNORM_SRGB";
		case DXGI_FORMAT_R16G16B16A16_FLOAT:	return "R16G16B16A16_FLOAT";
		default:
			WARN("Unexpected DXGI_FORMAT %d", format);
			return "other";
		}
	}

	uint BytesPerPixel(DXGI_FORMAT format)
	{
		// !!!UNDONE: handle *all* DXGI formats here

		switch (format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM:		return 4;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:	return 4;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:	return 8;
		default:
			WARN("Unexpected DXGI_FORMAT %d", format);
			return 0;
		}
	}
}
