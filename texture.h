#pragma once

namespace Framework
{
	enum TEXFLAG
	{
		TEXFLAG_Mipmaps		= 0x01,
		TEXFLAG_EnableUAV	= 0x02,
		
		TEXFLAG_Default		= 0x00,
	};

	class Texture2D
	{
	public:
				Texture2D();

		void	Init(
					ID3D11Device * pDevice,
					int2_arg dims,
					DXGI_FORMAT format,
					int flags = TEXFLAG_Default);
		void	Release();

		comptr<ID3D11Texture2D>				m_pTex;
		comptr<ID3D11ShaderResourceView>	m_pSrv;
		comptr<ID3D11UnorderedAccessView>	m_pUav;
		int2								m_dims;
		int									m_mipLevels;
		DXGI_FORMAT							m_format;
	};

	class TextureCube
	{
	public:
				TextureCube();

		void	Init(
					ID3D11Device * pDevice,
					int cubeSize,
					DXGI_FORMAT format,
					int flags = TEXFLAG_Default);
		void	Release();

		comptr<ID3D11Texture2D>				m_pTex;
		comptr<ID3D11ShaderResourceView>	m_pSrv;
		comptr<ID3D11UnorderedAccessView>	m_pUav;
		int									m_cubeSize;
		int									m_mipLevels;
		DXGI_FORMAT							m_format;
	};

	class Texture3D
	{
	public:
				Texture3D();

		void	Init(
					ID3D11Device * pDevice,
					int3_arg dims,
					DXGI_FORMAT format,
					int flags = TEXFLAG_Default);
		void	Release();

		comptr<ID3D11Texture3D>				m_pTex;
		comptr<ID3D11ShaderResourceView>	m_pSrv;
		comptr<ID3D11UnorderedAccessView>	m_pUav;
		int3								m_dims;
		int									m_mipLevels;
		DXGI_FORMAT							m_format;
	};



	// Texture loading

	enum TEXLOADFLAG
	{
		TEXLOADFLAG_Mipmap		= 0x1,		// Load mipmaps, or generate if not present in file
		TEXLOADFLAG_SRGB		= 0x2,		// Interpret image data as SRGB
		TEXLOADFLAG_HDR			= 0x4,		// Store image in float16 format

		TEXLOADFLAG_Default		= TEXLOADFLAG_Mipmap | TEXLOADFLAG_SRGB,
	};

	bool LoadTexture2D(
		ID3D11Device * pDevice,
		const char * path,
		Texture2D * pTexOut,
		int flags = TEXLOADFLAG_Default);

	bool LoadTextureCube(
		ID3D11Device * pDevice,
		const char * path,
		TextureCube * pTexOut,
		int flags = TEXLOADFLAG_Default);

	void CreateTexture1x1(
		ID3D11Device * pDevice,
		rgba_arg color,
		Texture2D * pTexOut,
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

	void CreateTextureCube1x1(
		ID3D11Device * pDevice,
		rgba_arg color,
		TextureCube * pTexOut,
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

	void CreateTexture2DFromMemory(
		ID3D11Device * pDevice,
		int2_arg dims,
		DXGI_FORMAT format,
		const void * pPixels,
		Texture2D * pTexOut);

	const char * NameOfFormat(DXGI_FORMAT format);
	int BitsPerPixel(DXGI_FORMAT format);
}
