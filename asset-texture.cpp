#include "framework.h"
#include "miniz.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4100 4189 4244 4702)	// unreferenced param, unreferenced local, integer down-convert, unreachable code
#include "stb_image_resize.h"
#pragma warning(pop)

namespace Framework
{
	// Infrastructure for compiling textures.
	//  * All textures are currently in RGBA8 sRGB format, top-down.
	//  * Textures are either stored raw, or with mips.  Textures with mips are also
	//      resampled up to the next pow2 size if necessary.
	//  * Textures are written to the archive as .bmps, for ease of debugging.
	//      That won't be true anymore when we have more general formats.
	//  * !!!UNDONE: Premultiplied alpha
	//  * !!!UNDONE: BCn compression
	//  * !!!UNDONE: Other pixel formats: HDR textures, normal maps, etc.
	//  * !!!UNDONE: Cubemaps, volume textures, sparse tiled textures, etc.

	namespace TextureCompiler
	{
		// Prototype various helper functions
		static bool WriteBMPToZip(
			const char * assetPath,
			int mipLevel,
			const byte4 * pPixels,
			int2_arg dims,
			mz_zip_archive * pZipOut);
	}



	// Compiler entry points

	bool CompileTextureRawAsset(
		const AssetCompileInfo * pACI,
		mz_zip_archive * pZipOut)
	{
		ASSERT_ERR(pACI);
		ASSERT_ERR(pACI->m_pathSrc);
		ASSERT_ERR(pACI->m_ack == ACK_TextureRaw);
		ASSERT_ERR(pZipOut);

		using namespace TextureCompiler;

		LOG("Compiling raw texture asset %s...", pACI->m_pathSrc);

		// Load the image
		int2 dims;
		int numComponents;
		byte4 * pPixels = (byte4 *)stbi_load(pACI->m_pathSrc, &dims.x, &dims.y, &numComponents, 4);
		if (!pPixels)
		{
			ERR("Couldn't load file %s: %s", pACI->m_pathSrc, stbi_failure_reason());
			return false;
		}

		// Store it as a .bmp
		if (!WriteBMPToZip(pACI->m_pathSrc, 0, pPixels, dims, pZipOut))
		{
			stbi_image_free(pPixels);
			return false;
		}

		stbi_image_free(pPixels);
		return true;
	}

	bool CompileTextureWithMipsAsset(
		const AssetCompileInfo * pACI,
		mz_zip_archive * pZipOut)
	{
		ASSERT_ERR(pACI);
		ASSERT_ERR(pACI->m_pathSrc);
		ASSERT_ERR(pACI->m_ack == ACK_TextureWithMips);
		ASSERT_ERR(pZipOut);

		using namespace TextureCompiler;

		LOG("Compiling mipmapped texture asset %s...", pACI->m_pathSrc);

		// Load the image
		int2 dims;
		int numComponents;
		byte4 * pPixels = (byte4 *)stbi_load(pACI->m_pathSrc, &dims.x, &dims.y, &numComponents, 4);
		if (!pPixels)
		{
			ERR("Couldn't load file %s: %s", pACI->m_pathSrc, stbi_failure_reason());
			return false;
		}

		// Resample the base mip up to pow2 if necessary
		int2 dimsBase;
		std::vector<byte4> pixelsBase;
		byte4 * pPixelsBase;
		if (!ispow2(dims.x) || !ispow2(dims.y))
		{
			dimsBase = makeint2(pow2_ceil(dims.x), pow2_ceil(dims.y));
			pixelsBase.resize(dimsBase.x * dimsBase.y);
			pPixelsBase = &pixelsBase[0];

			CHECK_ERR(stbir_resize_uint8_srgb(
						(const byte *)pPixels, dims.x, dims.y, 0,
						(byte *)pPixelsBase, dimsBase.x, dimsBase.y, 0,
						4, 3, 0));
		}
		else
		{
			dimsBase = dims;
			pPixelsBase = pPixels;
		}

		// Store the base level as a .bmp
		if (!WriteBMPToZip(pACI->m_pathSrc, 0, pPixelsBase, dimsBase, pZipOut))
		{
			stbi_image_free(pPixels);
			return false;
		}

		// Generate mip levels
		int mipLevels = log2_floor(maxComponent(dimsBase)) + 1;
		std::vector<byte4> pixelsMip;
		for (int level = 1; level < mipLevels; ++level)
		{
			int2 dimsMip = { max(dimsBase.x >> level, 1), max(dimsBase.y >> level, 1) };
			pixelsMip.resize(dimsMip.x * dimsMip.y);
			byte4 * pPixelsMip = &pixelsMip[0];

			CHECK_ERR(stbir_resize_uint8_srgb(
						(const byte *)pPixels, dims.x, dims.y, 0,
						(byte *)pPixelsMip, dimsMip.x, dimsMip.y, 0,
						4, 3, 0));

			if (!WriteBMPToZip(pACI->m_pathSrc, level, pPixelsMip, dimsMip, pZipOut))
			{
				stbi_image_free(pPixels);
				return false;
			}
		}

		stbi_image_free(pPixels);
		return true;
	}



	namespace TextureCompiler
	{
		static bool WriteBMPToZip(
			const char * assetPath,
			int mipLevel,
			const byte4 * pPixels,
			int2_arg dims,
			mz_zip_archive * pZipOut)
		{
			ASSERT_ERR(assetPath);
			ASSERT_ERR(mipLevel >= 0);
			ASSERT_ERR(pPixels);
			ASSERT_ERR(all(dims > 0));
			ASSERT_ERR(pZipOut);

			// Compose the .bmp headers
			BITMAPFILEHEADER bfh =
			{
				0x4d42,		// "BM"
				0, 0, 0,
				sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER),
			};
			BITMAPINFOHEADER bih =
			{
				sizeof(BITMAPINFOHEADER),
				dims.x, -dims.y,	// Negative height makes it go top-down
				1, 32, BI_RGB,
			};

			// Allocate a buffer to compose the .bmp file
			int imageSizeBytes = dims.x * dims.y * sizeof(byte4);
			int totalSizeBytes = sizeof(bfh) + sizeof(bih) + imageSizeBytes;
			std::vector<byte> buffer(totalSizeBytes);

			// Compose the file
			memcpy(&buffer[0], &bfh, sizeof(bfh));
			memcpy(&buffer[sizeof(bfh)], &bih, sizeof(bih));
			for (int i = 0, c = dims.x * dims.y; i < c; ++i)
			{
				byte4 rgba = pPixels[i];
				byte4 bgra = { rgba.b, rgba.g, rgba.r, rgba.a };
				*(byte4 *)&buffer[sizeof(bfh) + sizeof(bih) + i * sizeof(byte4)] = bgra;
			}

			// Compose the suffix
			char suffix[16] = {};
			sprintf_s(suffix, "/%d.bmp", mipLevel);

			// Write it to the .zip archive
			return WriteAssetDataToZip(assetPath, suffix, &buffer[0], totalSizeBytes, pZipOut);
		}
	}
}
