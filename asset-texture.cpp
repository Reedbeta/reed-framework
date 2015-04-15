#include "framework.h"
#include "asset-internal.h"

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
	//  * Enable the WRITE_BMP define to additionally write out all images as .bmps
	//      in the archive, for debugging.
	//  * !!!UNDONE: Premultiplied alpha
	//  * !!!UNDONE: BCn compression
	//  * !!!UNDONE: Other pixel formats: HDR textures, normal maps, etc.
	//  * !!!UNDONE: Cubemaps, volume textures, sparse tiled textures, etc.

#define WRITE_BMP 0

	namespace TextureCompiler
	{
		static const char * s_suffixMeta = "/meta";

		struct Meta
		{
			int2			m_dims;
			int				m_mipLevels;
			DXGI_FORMAT		m_format;
		};

		// Prototype various helper functions
		static bool WriteImageToZip(
			const char * assetPath,
			int mipLevel,
			const byte4 * pPixels,
			int2_arg dims,
			mz_zip_archive * pZipOut);

#if WRITE_BMP
		static bool WriteBMPToZip(
			const char * assetPath,
			int mipLevel,
			const byte4 * pPixels,
			int2_arg dims,
			mz_zip_archive * pZipOut);
#endif
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

		using namespace AssetCompiler;
		using namespace TextureCompiler;

		// Load the image
		int2 dims;
		int numComponents;
		byte4 * pPixels = (byte4 *)stbi_load(pACI->m_pathSrc, &dims.x, &dims.y, &numComponents, 4);
		if (!pPixels)
		{
			WARN("Couldn't load file %s: %s", pACI->m_pathSrc, stbi_failure_reason());
			return false;
		}

		// Fill out the metadata struct
		Meta meta =
		{
			dims,
			1,		// mipLevels
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		};

		// Write the data out to the archive
		if (!WriteAssetDataToZip(pACI->m_pathSrc, s_suffixMeta, &meta, sizeof(meta), pZipOut) ||
			!WriteImageToZip(pACI->m_pathSrc, 0, pPixels, dims, pZipOut))
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

		using namespace AssetCompiler;
		using namespace TextureCompiler;

		// Load the image
		int2 dims;
		int numComponents;
		byte4 * pPixels = (byte4 *)stbi_load(pACI->m_pathSrc, &dims.x, &dims.y, &numComponents, 4);
		if (!pPixels)
		{
			WARN("Couldn't load file %s: %s", pACI->m_pathSrc, stbi_failure_reason());
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

		// Fill out the metadata struct
		int mipLevels = log2_floor(maxComponent(dimsBase)) + 1;
		Meta meta =
		{
			dimsBase,
			mipLevels,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		};

		// Store the metadata and the base level pixels
		if (!WriteAssetDataToZip(pACI->m_pathSrc, s_suffixMeta, &meta, sizeof(meta), pZipOut) ||
			!WriteImageToZip(pACI->m_pathSrc, 0, pPixelsBase, dimsBase, pZipOut))
		{
			stbi_image_free(pPixels);
			return false;
		}

		// Generate mip levels
		std::vector<byte4> pixelsMip;
		for (int level = 1; level < mipLevels; ++level)
		{
			int2 dimsMip = CalculateMipDims(dimsBase, level);
			pixelsMip.resize(dimsMip.x * dimsMip.y);
			byte4 * pPixelsMip = &pixelsMip[0];

			CHECK_ERR(stbir_resize_uint8_srgb(
						(const byte *)pPixels, dims.x, dims.y, 0,
						(byte *)pPixelsMip, dimsMip.x, dimsMip.y, 0,
						4, 3, 0));

			if (!WriteImageToZip(pACI->m_pathSrc, level, pPixelsMip, dimsMip, pZipOut))
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
		static bool WriteImageToZip(
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

			// Compose the suffix
			char suffix[16] = {};
			sprintf_s(suffix, "/%d", mipLevel);

#if WRITE_BMP
			// Write a .bmp version of it, too, if we're doing that
			if (!WriteBMPToZip(assetPath, mipLevel, pPixels, dims, pZipOut))
				return false;
#endif

			// Write it to the .zip archive
			int sizeBytes = dims.x * dims.y * sizeof(byte4);
			return AssetCompiler::WriteAssetDataToZip(assetPath, suffix, pPixels, sizeBytes, pZipOut);
		}

#if WRITE_BMP
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

			std::vector<byte> buffer;
			WriteBMPToMemory(pPixels, dims, &buffer);

			// Compose the suffix
			char suffix[16] = {};
			sprintf_s(suffix, "/%d.bmp", mipLevel);

			// Write it to the .zip archive
			return AssetCompiler::WriteAssetDataToZip(assetPath, suffix, &buffer[0], buffer.size(), pZipOut);
		}
#endif // WRITE_BMP
	}



	// Load compiled data into a runtime game object

	bool LoadTexture2DFromAssetPack(
		AssetPack * pPack,
		const char * path,
		Texture2D * pTexOut)
	{
		ASSERT_ERR(pPack);
		ASSERT_ERR(path);
		ASSERT_ERR(pTexOut);

		using namespace TextureCompiler;

		pTexOut->m_pPack = pPack;

		// Look for the metadata in the asset pack
		Meta * pMeta;
		int metaSize;
		if (!pPack->LookupFile(path, s_suffixMeta, (void **)&pMeta, &metaSize))
		{
			WARN("Couldn't find metadata for texture %s in asset pack %s", path, pPack->m_path.c_str());
			return false;
		}
		if (metaSize != sizeof(Meta))
		{
			WARN("Metadata for texture %s in asset pack %s is wrong size, %d bytes (expected %d)",
				path, pPack->m_path.c_str(), metaSize, sizeof(Meta));
			return false;
		}
		pTexOut->m_dims = pMeta->m_dims;
		pTexOut->m_mipLevels = pMeta->m_mipLevels;
		pTexOut->m_format = pMeta->m_format;

		// Look for the individual mipmaps
		pTexOut->m_apPixels.resize(pTexOut->m_mipLevels);
		for (int i = 0; i < pTexOut->m_mipLevels; ++i)
		{
			// Compose the suffix
			char suffix[16] = {};
			sprintf_s(suffix, "/%d", i);

			int pixelsSize;
			if (!pPack->LookupFile(path, suffix, &pTexOut->m_apPixels[i], &pixelsSize))
			{
				WARN("Couldn't find mip level %d of texture %s in asset pack %s", i, path, pPack->m_path.c_str());
				return false;
			}
			int2 mipDims = CalculateMipDims(pMeta->m_dims, i);
			int expectedPixelsSize = mipDims.x * mipDims.y * BitsPerPixel(pMeta->m_format) / 8;
			if (pixelsSize != expectedPixelsSize)
			{
				WARN("Mip level %d of texture %s in asset pack %s is wrong size, %d bytes (expected %d)",
					i, path, pPack->m_path.c_str(), pixelsSize, expectedPixelsSize);
				return false;
			}
		}

		LOG("Loaded %s from asset pack %s - %dx%d, %d mips, %s",
			path, pPack->m_path.c_str(),
			pTexOut->m_dims.x, pTexOut->m_dims.y,
			pTexOut->m_mipLevels, NameOfFormat(pTexOut->m_format));

		return true;
	}



	// Create a library of all the textures in an asset pack

	bool LoadTextureLibFromAssetPack(
		AssetPack * pPack,
		const AssetCompileInfo * assets,
		int numAssets,
		TextureLib * pTexLibOut)
	{
		ASSERT_ERR(pPack);
		ASSERT_ERR(assets);
		ASSERT_ERR(numAssets > 0);
		ASSERT_ERR(pTexLibOut);

		// Find all the texture assets
		for (int i = 0; i < numAssets; ++i)
		{
			const AssetCompileInfo * pACI = &assets[i];
			if (pACI->m_ack != ACK_TextureRaw &&
				pACI->m_ack != ACK_TextureWithMips)
			{
				continue;
			}

			// !!!HACK: only use the basename for now, so materials can find textures
			const char * basename = pACI->m_pathSrc;
			if (const char * pLastSlash = strrchr(pACI->m_pathSrc, '/'))
				basename = pLastSlash + 1;

			auto iterAndBool = pTexLibOut->m_texs.insert(std::make_pair(std::string(basename), Texture2D()));

			if (!LoadTexture2DFromAssetPack(pPack, pACI->m_pathSrc, &iterAndBool.first->second))
			{
				pTexLibOut->m_texs.erase(iterAndBool.first);
				return false;
			}
		}

		return true;
	}



	// Helper function for quick and dirty apps - just compile and load a texture in one step.
	// Totally unnecessarily serializing, compressing, decompressing, and deserializing
	// all the data here...but whatever.

	bool LoadTexture2DRaw(
		const char * path,
		Texture2D * pTexOut)
	{
		ASSERT_ERR(path);
		ASSERT_ERR(pTexOut);

		// Set up an in-memory zip stream
		mz_zip_archive zipWrite = {};
		CHECK_ERR(mz_zip_writer_init_heap(&zipWrite, 0, 0));

		// Compile the mesh to it
		AssetCompileInfo aci = { path, ACK_TextureRaw };
		if (!AssetCompiler::CompileFullAssetPackToZip(&aci, 1, &zipWrite))
		{
			mz_zip_writer_end(&zipWrite);
			return false;
		}

		void * pData;
		size_t sizeBytes;
		if (!mz_zip_writer_finalize_heap_archive(&zipWrite, &pData, &sizeBytes))
		{
			WARN("Couldn't finalize archive");
			mz_zip_writer_end(&zipWrite);
			return false;
		}

		// Turn around and load the pack right back in

		mz_zip_archive zipRead = {};
		CHECK_ERR(mz_zip_reader_init_mem(&zipRead, pData, sizeBytes, 0));

		AssetPack * pPack = new AssetPack;
		pPack->m_path = "(in memory)";

		if (!AssetCompiler::LoadAssetPackFromZip(&zipRead, pPack))
		{
			mz_zip_writer_end(&zipRead);
			mz_zip_writer_end(&zipWrite);
			return false;
		}

		mz_zip_writer_end(&zipRead);
		mz_zip_writer_end(&zipWrite);

		// And extract the mesh from it
		return LoadTexture2DFromAssetPack(pPack, path, pTexOut);
	}
}
