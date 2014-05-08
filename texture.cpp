#include "framework.h"
#include <d3dx11.h>

namespace Framework
{
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

		D3D11_SUBRESOURCE_DATA initialData = { pPixels, width * BitsPerPixel(format) / 8 };
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
		static const char * s_names[] =
		{
			"UNKNOWN",
			"R32G32B32A32_TYPELESS",
			"R32G32B32A32_FLOAT",
			"R32G32B32A32_UINT",
			"R32G32B32A32_SINT",
			"R32G32B32_TYPELESS",
			"R32G32B32_FLOAT",
			"R32G32B32_UINT",
			"R32G32B32_SINT",
			"R16G16B16A16_TYPELESS",
			"R16G16B16A16_FLOAT",
			"R16G16B16A16_UNORM",
			"R16G16B16A16_UINT",
			"R16G16B16A16_SNORM",
			"R16G16B16A16_SINT",
			"R32G32_TYPELESS",
			"R32G32_FLOAT",
			"R32G32_UINT",
			"R32G32_SINT",
			"R32G8X24_TYPELESS",
			"D32_FLOAT_S8X24_UINT",
			"R32_FLOAT_X8X24_TYPELESS",
			"X32_TYPELESS_G8X24_UINT",
			"R10G10B10A2_TYPELESS",
			"R10G10B10A2_UNORM",
			"R10G10B10A2_UINT",
			"R11G11B10_FLOAT",
			"R8G8B8A8_TYPELESS",
			"R8G8B8A8_UNORM",
			"R8G8B8A8_UNORM_SRGB",
			"R8G8B8A8_UINT",
			"R8G8B8A8_SNORM",
			"R8G8B8A8_SINT",
			"R16G16_TYPELESS",
			"R16G16_FLOAT",
			"R16G16_UNORM",
			"R16G16_UINT",
			"R16G16_SNORM",
			"R16G16_SINT",
			"R32_TYPELESS",
			"D32_FLOAT",
			"R32_FLOAT",
			"R32_UINT",
			"R32_SINT",
			"R24G8_TYPELESS",
			"D24_UNORM_S8_UINT",
			"R24_UNORM_X8_TYPELESS",
			"X24_TYPELESS_G8_UINT",
			"R8G8_TYPELESS",
			"R8G8_UNORM",
			"R8G8_UINT",
			"R8G8_SNORM",
			"R8G8_SINT",
			"R16_TYPELESS",
			"R16_FLOAT",
			"D16_UNORM",
			"R16_UNORM",
			"R16_UINT",
			"R16_SNORM",
			"R16_SINT",
			"R8_TYPELESS",
			"R8_UNORM",
			"R8_UINT",
			"R8_SNORM",
			"R8_SINT",
			"A8_UNORM",
			"R1_UNORM",
			"R9G9B9E5_SHAREDEXP",
			"R8G8_B8G8_UNORM",
			"G8R8_G8B8_UNORM",
			"BC1_TYPELESS",
			"BC1_UNORM",
			"BC1_UNORM_SRGB",
			"BC2_TYPELESS",
			"BC2_UNORM",
			"BC2_UNORM_SRGB",
			"BC3_TYPELESS",
			"BC3_UNORM",
			"BC3_UNORM_SRGB",
			"BC4_TYPELESS",
			"BC4_UNORM",
			"BC4_SNORM",
			"BC5_TYPELESS",
			"BC5_UNORM",
			"BC5_SNORM",
			"B5G6R5_UNORM",
			"B5G5R5A1_UNORM",
			"B8G8R8A8_UNORM",
			"B8G8R8X8_UNORM",
			"R10G10B10_XR_BIAS_A2_UNORM",
			"B8G8R8A8_TYPELESS",
			"B8G8R8A8_UNORM_SRGB",
			"B8G8R8X8_TYPELESS",
			"B8G8R8X8_UNORM_SRGB",
			"BC6H_TYPELESS",
			"BC6H_UF16",
			"BC6H_SF16",
			"BC7_TYPELESS",
			"BC7_UNORM",
			"BC7_UNORM_SRGB",
			"AYUV",
			"Y410",
			"Y416",
			"NV12",
			"P010",
			"P016",
			"420_OPAQUE",
			"YUY2",
			"Y210",
			"Y216",
			"NV11",
			"AI44",
			"IA44",
			"P8",
			"A8P8",
			"B4G4R4A4_UNORM",
		};

		if (uint(format) >= dim(s_names))
		{
			WARN("Unexpected DXGI_FORMAT %d", format);
			return "UNKNOWN";
		}

		return s_names[format];
	}

	uint BitsPerPixel(DXGI_FORMAT format)
	{
		static const uint s_bitsPerPixel[] =
		{
			0,			// UNKNOWN
			128,		// R32G32B32A32_TYPELESS
			128,		// R32G32B32A32_FLOAT
			128,		// R32G32B32A32_UINT
			128,		// R32G32B32A32_SINT
			96,			// R32G32B32_TYPELESS
			96,			// R32G32B32_FLOAT
			96,			// R32G32B32_UINT
			96,			// R32G32B32_SINT
			64,			// R16G16B16A16_TYPELESS
			64,			// R16G16B16A16_FLOAT
			64,			// R16G16B16A16_UNORM
			64,			// R16G16B16A16_UINT
			64,			// R16G16B16A16_SNORM
			64,			// R16G16B16A16_SINT
			64,			// R32G32_TYPELESS
			64,			// R32G32_FLOAT
			64,			// R32G32_UINT
			64,			// R32G32_SINT
			64,			// R32G8X24_TYPELESS
			64,			// D32_FLOAT_S8X24_UINT
			64,			// R32_FLOAT_X8X24_TYPELESS
			64,			// X32_TYPELESS_G8X24_UINT
			32,			// R10G10B10A2_TYPELESS
			32,			// R10G10B10A2_UNORM
			32,			// R10G10B10A2_UINT
			32,			// R11G11B10_FLOAT
			32,			// R8G8B8A8_TYPELESS
			32,			// R8G8B8A8_UNORM
			32,			// R8G8B8A8_UNORM_SRGB
			32,			// R8G8B8A8_UINT
			32,			// R8G8B8A8_SNORM
			32,			// R8G8B8A8_SINT
			32,			// R16G16_TYPELESS
			32,			// R16G16_FLOAT
			32,			// R16G16_UNORM
			32,			// R16G16_UINT
			32,			// R16G16_SNORM
			32,			// R16G16_SINT
			32,			// R32_TYPELESS
			32,			// D32_FLOAT
			32,			// R32_FLOAT
			32,			// R32_UINT
			32,			// R32_SINT
			32,			// R24G8_TYPELESS
			32,			// D24_UNORM_S8_UINT
			32,			// R24_UNORM_X8_TYPELESS
			32,			// X24_TYPELESS_G8_UINT
			16,			// R8G8_TYPELESS
			16,			// R8G8_UNORM
			16,			// R8G8_UINT
			16,			// R8G8_SNORM
			16,			// R8G8_SINT
			16,			// R16_TYPELESS
			16,			// R16_FLOAT
			16,			// D16_UNORM
			16,			// R16_UNORM
			16,			// R16_UINT
			16,			// R16_SNORM
			16,			// R16_SINT
			8,			// R8_TYPELESS
			8,			// R8_UNORM
			8,			// R8_UINT
			8,			// R8_SNORM
			8,			// R8_SINT
			8,			// A8_UNORM
			1,			// R1_UNORM
			32,			// R9G9B9E5_SHAREDEXP
			16,			// R8G8_B8G8_UNORM
			16,			// G8R8_G8B8_UNORM
			4,			// BC1_TYPELESS
			4,			// BC1_UNORM
			4,			// BC1_UNORM_SRGB
			8,			// BC2_TYPELESS
			8,			// BC2_UNORM
			8,			// BC2_UNORM_SRGB
			8,			// BC3_TYPELESS
			8,			// BC3_UNORM
			8,			// BC3_UNORM_SRGB
			4,			// BC4_TYPELESS
			4,			// BC4_UNORM
			4,			// BC4_SNORM
			8,			// BC5_TYPELESS
			8,			// BC5_UNORM
			8,			// BC5_SNORM
			16,			// B5G6R5_UNORM
			16,			// B5G5R5A1_UNORM
			32,			// B8G8R8A8_UNORM
			32,			// B8G8R8X8_UNORM
			32,			// R10G10B10_XR_BIAS_A2_UNORM
			32,			// B8G8R8A8_TYPELESS
			32,			// B8G8R8A8_UNORM_SRGB
			32,			// B8G8R8X8_TYPELESS
			32,			// B8G8R8X8_UNORM_SRGB
			8,			// BC6H_TYPELESS
			8,			// BC6H_UF16
			8,			// BC6H_SF16
			8,			// BC7_TYPELESS
			8,			// BC7_UNORM
			8,			// BC7_UNORM_SRGB
			
			// NOTE: I don't know what are the bit depths for the video formats;
			// the MS docs don't specify them well.

			0,			// AYUV
			0,			// Y410
			0,			// Y416
			0,			// NV12
			0,			// P010
			0,			// P016
			0,			// 420_OPAQUE
			0,			// YUY2
			0,			// Y210
			0,			// Y216
			0,			// NV11
			0,			// AI44
			0,			// IA44

			8,			// P8
			16,			// A8P8
			16,			// B4G4R4A4_UNORM
		};

		if (uint(format) >= dim(s_bitsPerPixel))
		{
			WARN("Unexpected DXGI_FORMAT %d", format);
			return 0;
		}

		return s_bitsPerPixel[format];
	}
}
