#include "framework.h"
#include "miniz.h"

#include <sys/types.h>
#include <sys/stat.h>

namespace Framework
{
	// AssetPack implementation

	AssetPack::AssetPack()
	{
	}

	bool AssetPack::LookupFile(const char * path, const char * suffix, void ** ppDataOut, int * pSizeOut)
	{
		ASSERT_ERR(path);

		std::string fullPath = path;
		if (suffix)
			fullPath += suffix;
		CHECK_WARN(CheckPathChars(fullPath.c_str()));
		auto iter = m_directory.find(fullPath);
		if (iter == m_directory.end())
			return false;

		int iFile = iter->second;
		const FileInfo & fileinfo = m_files[iFile];

		if (ppDataOut)
			*ppDataOut = (fileinfo.m_size > 0) ? &m_data[fileinfo.m_offset] : nullptr;
		if (pSizeOut)
			*pSizeOut = fileinfo.m_size;

		return true;
	}

	void AssetPack::Reset()
	{
		m_data.clear();
		m_files.clear();
		m_directory.clear();
		m_path.clear();
	}


	
	// Infrastructure for compiling art source files (such as Wavefront .obj meshes, and
	// textures in .bmp/.psd/whatever) to engine-friendly data (such as vertex/index buffers,
	// and RGBA8 pixel data with pre-generated mipmaps).
	//  * Takes a list of source files to compile.  Current assumption is 1 source file == 1 asset.
	//  * Compiled data is stored as a set of files in a .zip.  The source file path is used
	//      as a directory name in the .zip.  For example, source file "foo/bar/baz.obj" will
	//      result in a directory "foo/bar/baz.obj/" with files in it for verts, indices, etc.
	//  * Compiled data is considered out-of-date and recompiled if the mod time of the source
	//      file is newer than the mod time of the asset pack (the .zip).
	//  * Version numbers for the whole pack system and each asset type are also stored in the
	//      .zip, and mismatches will trigger recompilation.
	//  !!!UNDONE: build the list of sources to compile by following dependencies from some root.

	namespace AssetCompiler
	{
		static const char * s_pathVersionInfo = "version";

		enum PACKVER
		{
			PACKVER_Current = 1,
		};

		enum MESHVER
		{
			MESHVER_Current = 1,
		};

		enum TEXVER
		{
			TEXVER_Current = 1,
		};

		struct VersionInfo
		{
			PACKVER		m_packver;
			MESHVER		m_meshver;
			TEXVER		m_texver;
		};

		// Prototype various helper functions

		// Compile an entire asset pack from scratch.
		static bool CompileFullAssetPack(
			const char * packPath,
			const AssetCompileInfo * assets,
			int numAssets);

		// Check if any assets in a pack are out of date by version number or mod time,
		// returning a list of ones that need updating.  Return flag indicates whether
		// operation succeeded.
		static bool FindOutOfDateAssets(
			const char * packPath,
			const AssetCompileInfo * assets,
			int numAssets,
			std::vector<AssetCompileInfo> * pAssetsToUpdateOut);

		// Update an asset pack in-place by recompiling the given assets,
		// preserving any other data already in the pack.
		static bool UpdateAssetPack(
			const char * packPath,
			const AssetCompileInfo * assetsToUpdate,
			int numAssets);
	}

	// Prototype individual compilation functions for different asset types

	bool CompileOBJMeshAsset(
		const AssetCompileInfo * pACI,
		mz_zip_archive * pZipOut);
	bool CompileTextureRawAsset(
		const AssetCompileInfo * pACI,
		mz_zip_archive * pZipOut);
	bool CompileTextureWithMipsAsset(
		const AssetCompileInfo * pACI,
		mz_zip_archive * pZipOut);

	typedef bool (*AssetCompileFunc)(const AssetCompileInfo *, mz_zip_archive *);
	static const AssetCompileFunc s_assetCompileFuncs[] =
	{
		&CompileOBJMeshAsset,				// ACK_OBJMesh
		&CompileTextureRawAsset,			// ACK_TextureRaw
		&CompileTextureWithMipsAsset,		// ACK_TextureWithMips
	};
	cassert(dim(s_assetCompileFuncs) == ACK_Count);

	static const char * s_ackNames[] =
	{
		"OBJ mesh",							// ACK_OBJMesh
		"raw texture",						// ACK_TextureRaw
		"mipmapped texture",				// ACK_TextureWithMips
	};
	cassert(dim(s_ackNames) == ACK_Count);

	// Load an asset pack file, checking that all its assets are present and up to date,
	// and compiling any that aren't.
	bool LoadAssetPackOrCompileIfOutOfDate(
		const char * packPath,
		const AssetCompileInfo * assets,
		int numAssets,
		AssetPack * pPackOut)
	{
		ASSERT_ERR(packPath);
		ASSERT_ERR(assets);
		ASSERT_ERR(numAssets > 0);
		ASSERT_ERR(pPackOut);

		using namespace AssetCompiler;

		// Does the asset pack already exist?
		struct _stat packStat;
		if (_stat(packPath, &packStat) == 0)
		{
			// Check if any assets are out of date
			std::vector<AssetCompileInfo> assetsToUpdate;
			if (!FindOutOfDateAssets(packPath, assets, numAssets, &assetsToUpdate))
			{
				LOG("Asset pack %s exists but seems to be corrupt; recompiling it from sources.", packPath);
				if (!CompileFullAssetPack(packPath, assets, numAssets))
					return false;
			}
			else if (assetsToUpdate.empty())
			{
				LOG("Asset pack %s is up to date.", packPath);
			}
			else
			{
				LOG("Asset pack %s is out of date; updating.", packPath);
				if (!UpdateAssetPack(packPath, &assetsToUpdate[0], int(assetsToUpdate.size())))
					return false;
			}
		}
		else
		{
			LOG("Asset pack %s doesn't exist; compiling it from sources.", packPath);
			if (!CompileFullAssetPack(packPath, assets, numAssets))
				return false;
		}

		// It ought to exist and be up-to-date now, so load it
		return LoadAssetPack(packPath, pPackOut);
	}

	// Just load an asset pack file.
	bool LoadAssetPack(
		const char * packPath,
		AssetPack * pPackOut)
	{
		ASSERT_ERR(packPath);
		ASSERT_ERR(pPackOut);
		
		using namespace AssetCompiler;

		// Load the archive directory
		mz_zip_archive zip = {};
		if (!mz_zip_reader_init_file(&zip, packPath, 0))
		{
			WARN("Couldn't load asset pack %s", packPath);
			return false;
		}

		pPackOut->m_path = packPath;

		int numFiles = int(mz_zip_reader_get_num_files(&zip));
		pPackOut->m_files.resize(numFiles);
		pPackOut->m_directory.clear();
		pPackOut->m_directory.reserve(numFiles);

		// Run through all the files, build the file list and directory and sum up their sizes
		int bytesTotal = 0;
		for (int i = 0; i < numFiles; ++i)
		{
			mz_zip_archive_file_stat fileStat;
			if (!mz_zip_reader_file_stat(&zip, i, &fileStat))
			{
				WARN("Couldn't read directory entry %d of %d from asset pack %s", i, numFiles, packPath);
				mz_zip_reader_end(&zip);
				return false;
			}

			AssetPack::FileInfo * pFileInfo = &pPackOut->m_files[i];
			pFileInfo->m_path = fileStat.m_filename;
			pFileInfo->m_offset = bytesTotal;
			pFileInfo->m_size = int(fileStat.m_uncomp_size);

			pPackOut->m_directory.insert(std::make_pair(pFileInfo->m_path, i));

			bytesTotal += int(fileStat.m_uncomp_size);
		}

		// Allocate memory to store the decompressed data
		pPackOut->m_data.resize(bytesTotal);

		// Decompress all the files
		for (int i = 0; i < numFiles; ++i)
		{
			AssetPack::FileInfo * pFileInfo = &pPackOut->m_files[i];

			// Skip zero size files (trailing ones will cause an std::vector assert)
			if (pFileInfo->m_size == 0)
				continue;

			if (!mz_zip_reader_extract_to_mem(
					&zip, i,
					&pPackOut->m_data[pFileInfo->m_offset],
					pFileInfo->m_size, 0))
			{
				WARN("Couldn't extract file %s (index %d of %d) from asset pack %s", pFileInfo->m_path.c_str(), i, numFiles, packPath);
				mz_zip_reader_end(&zip);
				return false;
			}
		}

		mz_zip_reader_end(&zip);

		// Extract the version info
		VersionInfo * pVerInfo;
		int verInfoSize;
		if (!pPackOut->LookupFile(s_pathVersionInfo, nullptr, (void **)&pVerInfo, &verInfoSize))
		{
			WARN("Couldn't find version info in asset pack %s", packPath);
		}
		ASSERT_WARN(verInfoSize == sizeof(VersionInfo));

		// Check that all the versions are correct
		ASSERT_WARN_MSG(
			pVerInfo->m_packver == PACKVER_Current,
			"Asset pack %s has wrong pack version %d (expected %d)", packPath, pVerInfo->m_packver, PACKVER_Current);
		ASSERT_WARN_MSG(
			pVerInfo->m_meshver == MESHVER_Current,
			"Asset pack %s has wrong mesh version %d (expected %d)", packPath, pVerInfo->m_meshver, MESHVER_Current);
		ASSERT_WARN_MSG(
			pVerInfo->m_texver == TEXVER_Current,
			"Asset pack %s has wrong texture version %d (expected %d)", packPath, pVerInfo->m_texver, TEXVER_Current);

		LOG("Loaded asset pack %s - %dMB uncompressed", packPath, bytesTotal / 1048576);
		return true;
	}



	namespace AssetCompiler
	{
		// Compile an entire asset pack from scratch.
		static bool CompileFullAssetPack(
			const char * packPath,
			const AssetCompileInfo * assets,
			int numAssets)
		{
			ASSERT_ERR(packPath);
			ASSERT_ERR(assets);
			ASSERT_ERR(numAssets > 0);

			mz_zip_archive zip = {};
			if (!mz_zip_writer_init_file(&zip, packPath, 0))
			{
				WARN("Couldn't open %s for writing", packPath);
				return false;
			}

			// !!!UNDONE: not nicely generating entries in the .zip for directories in the internal paths.
			// Doesn't seem to matter as .zip viewers handle it fine, but maybe we should do that anyway?

			int numErrors = 0;
			for (int iAsset = 0; iAsset < numAssets; ++iAsset)
			{
				const AssetCompileInfo * pACI = &assets[iAsset];
				ACK ack = pACI->m_ack;
				ASSERT_ERR(ack >= 0 && ack < ACK_Count);
			
				LOG("[%d/%d] Compiling %s asset %s...", iAsset+1, numAssets, s_ackNames[ack], pACI->m_pathSrc);

				// Compile the asset
				if (s_assetCompileFuncs[ack](pACI, &zip))
				{
					// Write directory for the asset to the .zip as a sentinel that it compiled
					// !!!UNDONE
				}
				else
				{
					WARN("Couldn't compile asset %s", pACI->m_pathSrc);
					++numErrors;
				}
			}

			if (numErrors > 0)
			{
				WARN("Failed to compile %d of %d assets", numErrors, numAssets);
			}

			// Write version info
			VersionInfo version =
			{
				PACKVER_Current,
				MESHVER_Current,
				TEXVER_Current,
			};
			if (!WriteAssetDataToZip(s_pathVersionInfo, nullptr, &version, sizeof(version), &zip))
				return false;

			if (!mz_zip_writer_finalize_archive(&zip))
			{
				WARN("Couldn't finalize archive %s", packPath);
				mz_zip_writer_end(&zip);
				return false;
			}

			mz_zip_writer_end(&zip);

			return (numErrors == 0);
		}

		// Check if any assets in a pack are out of date by version number or mod time,
		// returning a list of ones that need updating.
		static bool FindOutOfDateAssets(
			const char * packPath,
			const AssetCompileInfo * assets,
			int numAssets,
			std::vector<AssetCompileInfo> * pAssetsToUpdateOut)
		{
			ASSERT_ERR(packPath);
			ASSERT_ERR(assets);
			ASSERT_ERR(numAssets > 0);
			ASSERT_ERR(pAssetsToUpdateOut);

			// Load the archive directory
			mz_zip_archive zip = {};
			if (!mz_zip_reader_init_file(&zip, packPath, 0))
			{
				WARN("Couldn't load asset pack %s", packPath);
				return false;
			}

			// Extract the version info
			VersionInfo ver;
			int fileIndex = mz_zip_reader_locate_file(&zip, s_pathVersionInfo, nullptr, 0);
			if (fileIndex < 0)
			{
				WARN("Couldn't find version info in asset pack %s", packPath);
				mz_zip_reader_end(&zip);
				return false;
			}
			if (!mz_zip_reader_extract_to_mem(&zip, fileIndex, &ver, sizeof(ver), 0))
			{
				WARN("Couldn't extract version info from asset pack %s", packPath);
				mz_zip_reader_end(&zip);
				return false;
			}

			// If the pack version is wrong, we have to recompile the whole thing
			if (ver.m_packver != PACKVER_Current)
			{
				pAssetsToUpdateOut->assign(assets, assets + numAssets);
				mz_zip_reader_end(&zip);
				return true;
			}

			// Get the mod date of the asset pack
			struct _stat packStat;
			CHECK_ERR(_stat(packPath, &packStat) == 0);

			// Go through the assets and check their individual versions and mod dates
			for (int i = 0; i < numAssets; ++i)
			{
				// Check the appropriate version number for the asset type
				const AssetCompileInfo * pACI = &assets[i];
				switch (pACI->m_ack)
				{
				case ACK_OBJMesh:
					if (ver.m_meshver != MESHVER_Current)
					{
						pAssetsToUpdateOut->push_back(*pACI);
						continue;
					}
					break;

				case ACK_TextureRaw:
				case ACK_TextureWithMips:
					if (ver.m_texver != TEXVER_Current)
					{
						pAssetsToUpdateOut->push_back(*pACI);
						continue;
					}
					break;

				default:
					ERR("Missing case for ACK %d", pACI->m_ack);
					break;
				}

				// Check if the asset actually exists in the pack.  If it doesn't, needs to be compiled.
				// !!!UNDONE
#if LATER
				if (mz_zip_reader_locate_file(&zip, pACI->m_pathSrc, nullptr, 0) < 0)
				{
					pAssetsToUpdateOut->push_back(*pACI);
					continue;
				}
#endif

				// Check mod time of the source file against that of the pack
				// If the source file doesn't exist, that's OK!  Asset packs can be
				// distributed in lieu of source files.
				struct _stat srcStat;
				if (_stat(pACI->m_pathSrc, &srcStat) == 0 &&
					srcStat.st_mtime > packStat.st_mtime)
				{
					pAssetsToUpdateOut->push_back(*pACI);
					continue;
				}
			}

			mz_zip_reader_end(&zip);
			return true;
		}

		// Update an asset pack in-place by recompiling the given assets,
		// preserving any other data already in the pack.
		static bool UpdateAssetPack(
			const char * packPath,
			const AssetCompileInfo * assetsToUpdate,
			int numAssets)
		{
			ASSERT_ERR(packPath);
			ASSERT_ERR(assetsToUpdate);
			ASSERT_ERR(numAssets > 0);
		
			// Load the archive directory
			mz_zip_archive zip = {};
			if (!mz_zip_reader_init_file(&zip, packPath, 0))
			{
				WARN("Couldn't load asset pack %s", packPath);
				return false;
			}

			// !!!UNDONE

			mz_zip_reader_end(&zip);
			return true;
		}
	}
}
