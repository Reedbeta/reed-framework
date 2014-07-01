#pragma once

namespace Framework
{
	// Utility functions for working with texture formats
	const char * NameOfFormat(DXGI_FORMAT format);
	int BitsPerPixel(DXGI_FORMAT format);

	// Utility functions for counting mips
	// Note: these don't take into account minimum block sizes for compressed formats.

	inline int CalculateMipCount(int size)
		{ return log2_floor(size) + 1; }
	inline int CalculateMipCount(int2_arg dims)
		{ return CalculateMipCount(maxComponent(dims)); }
	inline int CalculateMipCount(int3_arg dims)
		{ return CalculateMipCount(maxComponent(dims)); }

	inline int CalculateMipDims(int baseDim, int level)
		{ return max(baseDim >> level, 1); }
	inline int2 CalculateMipDims(int2_arg baseDims, int level)
		{ return max(makeint2(baseDims.x >> level, baseDims.y >> level), makeint2(1)); }
	inline int3 CalculateMipDims(int3_arg baseDims, int level)
		{ return max(makeint3(baseDims.x >> level, baseDims.y >> level, baseDims.z >> level), makeint3(1)); }

	inline int CalculateMipSizeInBytes(int baseDim, int level, DXGI_FORMAT format)
		{ return square(CalculateMipDims(baseDim, level)) * BitsPerPixel(format); }
	inline int CalculateMipSizeInBytes(int2_arg baseDims, int level, DXGI_FORMAT format)
		{ int2 mipDims = CalculateMipDims(baseDims, level); return mipDims.x * mipDims.y * BitsPerPixel(format); }
	inline int CalculateMipSizeInBytes(int3_arg baseDims, int level, DXGI_FORMAT format)
		{ int3 mipDims = CalculateMipDims(baseDims, level); return mipDims.x * mipDims.y * mipDims.z * BitsPerPixel(format); }

	inline int CalculateMipPyramidSizeInBytes(int baseDim, DXGI_FORMAT format, int mipLevels = -1)
	{
		if (mipLevels < 0)
			mipLevels = CalculateMipCount(baseDim);
		int total = 0;
		for (int i = 0; i < mipLevels; ++i)
			total += CalculateMipSizeInBytes(baseDim, i, format);
		return total;
	}
	inline int CalculateMipPyramidSizeInBytes(int2_arg baseDims, DXGI_FORMAT format, int mipLevels = -1)
	{
		if (mipLevels < 0)
			mipLevels = CalculateMipCount(baseDims);
		int total = 0;
		for (int i = 0; i < mipLevels; ++i)
			total += CalculateMipSizeInBytes(baseDims, i, format);
		return total;
	}
	inline int CalculateMipPyramidSizeInBytes(int3_arg baseDims, DXGI_FORMAT format, int mipLevels = -1)
	{
		if (mipLevels < 0)
			mipLevels = CalculateMipCount(baseDims);
		int total = 0;
		for (int i = 0; i < mipLevels; ++i)
			total += CalculateMipSizeInBytes(baseDims, i, format);
		return total;
	}



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

		int		SizeInBytes() const
					{ return CalculateMipPyramidSizeInBytes(m_dims, m_format, m_mipLevels); }

		// Read back the data to main memory - you're responsible for allocing enough
		void	Readback(
					ID3D11DeviceContext * pCtx,
					int level,
					void * pDataOut);

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

		int		SizeInBytes() const
					{ return CalculateMipPyramidSizeInBytes(m_cubeSize, m_format, m_mipLevels); }

		// Read back the data to main memory - you're responsible for allocing enough
		void	Readback(
					ID3D11DeviceContext * pCtx,
					int face,
					int level,
					void * pDataOut);

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

		int		SizeInBytes() const
					{ return CalculateMipPyramidSizeInBytes(m_dims, m_format, m_mipLevels); }

		// Read back the data to main memory - you're responsible for allocing enough
		void	Readback(
					ID3D11DeviceContext * pCtx,
					int level,
					void * pDataOut);

		comptr<ID3D11Texture3D>				m_pTex;
		comptr<ID3D11ShaderResourceView>	m_pSrv;
		comptr<ID3D11UnorderedAccessView>	m_pUav;
		int3								m_dims;
		int									m_mipLevels;
		DXGI_FORMAT							m_format;
	};



	// Texture loading from files

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

	// Creating textures directly in memory

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
}
