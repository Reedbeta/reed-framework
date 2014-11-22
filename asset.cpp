#include "framework.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "stb_image.h"
#include "stb_image_resize.h"
#include "stb_image_write.h"

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

namespace Framework
{
	// AssetPack implementation

	AssetPack::AssetPack()
	:	m_data(),
		m_directory()
	{
	}

	byte * AssetPack::LookupFile(const char * path)
	{
		ASSERT_ERR(path);

		// !!!UNDONE: assert that path is in a normal form (lowercase, forward slashes, etc.)?

		auto iter = m_directory.find(path);
		if (iter == m_directory.end())
			return nullptr;

		int iFile = iter->second;
		return &m_data[m_files[iFile].m_offset];
	}

	void AssetPack::Release()
	{
		m_data.clear();
		m_directory.clear();
	}


	
	// Asset loading & compilation

	// Load an asset pack file, checking that all its assets are present and up to date,
	// and compiling any that aren't.
	bool LoadAssetPackOrCompileIfOutOfDate(
		const char * packPath,
		const char ** assetPaths,
		int numAssets,
		AssetPack * pPackOut)
	{
		ASSERT_ERR(packPath);
		ASSERT_ERR(pPackOut);

		// Does the asset pack already exist?
		struct _stat packStat;
		if (_stat(packPath, &packStat) == 0)
		{
			// Ensure all assets are up to date
			if (!UpdateAssetPack(packPath, assetPaths, numAssets))
				return false;
		}
		else
		{
			// The pack file doesn't exist.  Compile it from the source assets.
			if (!CompileFullAssetPack(packPath, assetPaths, numAssets))
				return false;
		}

		// It ought to exist and be up-to-date now, so load it
		return LoadAssetPack(packPath, pPackOut);
	}

	// Just load an asset pack file, THAT'S ALL.
	bool LoadAssetPack(
		const char * packPath,
		AssetPack * pPackOut)
	{
		ASSERT_ERR(packPath);
		ASSERT_ERR(pPackOut);
		
		// Load the archive directory
		mz_zip_archive zip = {};
		if (!mz_zip_reader_init_file(&zip, packPath, 0))
		{
			ERR("Couldn't load asset pack %s", packPath);
			return false;
		}

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
				ERR("Couldn't read directory entry %d of %d from asset pack %s", i, numFiles, packPath);
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

			if (!mz_zip_reader_extract_to_mem(
					&zip, i,
					&pPackOut->m_data[pFileInfo->m_offset],
					pFileInfo->m_size, 0))
			{
				ERR("Couldn't extract file %s (index %d of %d) from asset pack %s", pFileInfo->m_path.c_str(), i, numFiles, packPath);
				mz_zip_reader_end(&zip);
				return false;
			}
		}

		mz_zip_reader_end(&zip);

		LOG("Loaded asset pack %s - %d files, %d total bytes", packPath, numFiles, bytesTotal);
		return true;
	}

	// Compile an entire asset pack from scratch.
	bool CompileFullAssetPack(
		const char * packPath,
		const char ** assetPaths,
		int numAssets)
	{
		ASSERT_ERR(packPath);
		ASSERT_ERR(assetPaths);
		ASSERT_ERR(numAssets > 0);
		
		// !!!UNDONE
		return false;
	}

	// Update an asset pack by compiling the assets that are out of date or missing from it.
	bool UpdateAssetPack(
		const char * packPath,
		const char ** assetPaths,
		int numAssets)
	{
		ASSERT_ERR(packPath);
		ASSERT_ERR(assetPaths);
		ASSERT_ERR(numAssets > 0);
		
		// Load the archive directory
		mz_zip_archive zip;
		CHECK_ERR(mz_zip_reader_init_file(&zip, packPath, 0));

		// !!!UNDONE
		return false;
	}
}
