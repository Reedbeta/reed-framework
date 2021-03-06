#include "framework.h"
#include "asset-internal.h"
#include <algorithm>

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
		CHECK_WARN(AssetCompiler::NormalizePath(const_cast<char *>(fullPath.data())));
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

	bool AssetPack::HasAsset(const char * path)
	{
		return (m_manifest.find(std::string(path)) != m_manifest.end());
	}

	void AssetPack::Reset()
	{
		m_data.clear();
		m_files.clear();
		m_directory.clear();
		m_manifest.clear();
		m_path.clear();
	}


	
	namespace AssetCompiler
	{
		static const char * s_pathVersionInfo = "version";
		static const char * s_pathManifest = "manifest";
	}

	// Prototype individual compilation functions for different asset types

	bool CompileOBJMeshAsset(
		const AssetCompileInfo * pACI,
		mz_zip_archive * pZipOut);
	bool CompileOBJMtlLibAsset(
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
		&CompileOBJMtlLibAsset,				// ACK_OBJMtlLib
		&CompileTextureRawAsset,			// ACK_TextureRaw
		&CompileTextureWithMipsAsset,		// ACK_TextureWithMips
	};
	cassert(dim(s_assetCompileFuncs) == ACK_Count);

	static const char * s_ackNames[] =
	{
		"OBJ mesh",							// ACK_OBJMesh
		"OBJ material library",				// ACK_OBJMtlLib
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
			std::vector<int> assetsToUpdate;
			if (!FindOutOfDateAssets(packPath, assets, numAssets, &assetsToUpdate))
			{
				LOG("Asset pack %s exists but seems to be corrupt; recompiling it from sources.", packPath);
				if (!CompileFullAssetPackToFile(packPath, assets, numAssets))
					return false;
			}
			else if (assetsToUpdate.empty())
			{
				LOG("Asset pack %s is up to date.", packPath);
			}
			else
			{
				LOG("Asset pack %s is out of date; updating.", packPath);
				if (!UpdateAssetPack(packPath, assets, numAssets, assetsToUpdate))
					return false;
			}
		}
		else
		{
			LOG("Asset pack %s doesn't exist; compiling it from sources.", packPath);
			if (!CompileFullAssetPackToFile(packPath, assets, numAssets))
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
		
		// Load the archive directory
		mz_zip_archive zip = {};
		if (!mz_zip_reader_init_file(&zip, packPath, 0))
		{
			WARN("Couldn't load asset pack %s", packPath);
			return false;
		}

		pPackOut->m_path = packPath;

		if (!AssetCompiler::LoadAssetPackFromZip(&zip, pPackOut))
		{
			mz_zip_reader_end(&zip);
			return false;
		}

		mz_zip_reader_end(&zip);

		LOG("Loaded asset pack %s - %dMB uncompressed", packPath, pPackOut->m_data.size() / 1048576);
		return true;
	}



	namespace AssetCompiler
	{
		// Load an asset pack file from a zip stream (can be in memory or a file).
		bool LoadAssetPackFromZip(
			mz_zip_archive * pZip,
			AssetPack * pPackOut)
		{
			ASSERT_ERR(pZip);
			ASSERT_ERR(pPackOut);

			const char * packPath = pPackOut->m_path.c_str();
		
			int numFiles = int(mz_zip_reader_get_num_files(pZip));
			pPackOut->m_files.resize(numFiles);
			pPackOut->m_directory.clear();
			pPackOut->m_directory.reserve(numFiles);

			// Run through all the files, build the file list and directory and sum up their sizes
			int bytesTotal = 0;
			for (int i = 0; i < numFiles; ++i)
			{
				mz_zip_archive_file_stat fileStat;
				if (!mz_zip_reader_file_stat(pZip, i, &fileStat))
				{
					WARN("Couldn't read directory entry %d of %d from asset pack %s", i, numFiles, packPath);
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
						pZip, i,
						&pPackOut->m_data[pFileInfo->m_offset],
						pFileInfo->m_size, 0))
				{
					WARN("Couldn't extract file %s (index %d of %d) from asset pack %s",
						pFileInfo->m_path.c_str(), i, numFiles, packPath);
					return false;
				}
			}

			// Extract the version info
			VersionInfo * pVerInfo;
			int verInfoSize;
			if (!pPackOut->LookupFile(s_pathVersionInfo, nullptr, (void **)&pVerInfo, &verInfoSize))
			{
				WARN("Couldn't find version info in asset pack %s", packPath);
				return false;
			}
			if (verInfoSize != sizeof(VersionInfo))
			{
				WARN("Version info in asset pack %s is wrong size, %d bytes (expected %d)",
					packPath, verInfoSize, sizeof(VersionInfo));
				return false;
			}

			// Check that all the versions are correct
			if (pVerInfo->m_packver != PACKVER_Current)
			{
				WARN("Asset pack %s has wrong pack version %d (expected %d)", packPath, pVerInfo->m_packver, PACKVER_Current);
				return false;
			}
			if (pVerInfo->m_meshver != MESHVER_Current)
			{
				WARN("Asset pack %s has wrong mesh version %d (expected %d)", packPath, pVerInfo->m_meshver, MESHVER_Current);
				return false;
			}
			if (pVerInfo->m_mtlver != MTLVER_Current)
			{
				WARN("Asset pack %s has wrong material version %d (expected %d)", packPath, pVerInfo->m_mtlver, MTLVER_Current);
				return false;
			}
			if (pVerInfo->m_texver != TEXVER_Current)
			{
				WARN("Asset pack %s has wrong texture version %d (expected %d)", packPath, pVerInfo->m_texver, TEXVER_Current);
				return false;
			}

			// Extract the manifest
			const char * pManifest;
			int manifestSize;
			if (!pPackOut->LookupFile(s_pathManifest, nullptr, (void **)&pManifest, &manifestSize))
			{
				WARN("Couldn't find manifest in asset pack %s", packPath);
				return false;
			}
			ParseManifest(pManifest, manifestSize, packPath, &pPackOut->m_manifest);

			return true;
		}

		// Check that filenames are printable-ASCII-only, lowercase, and there are no backslashes
		// (this should really be generalized to allow UTF-8 printable chars)
		bool NormalizePath(char * path)
		{
			ASSERT_ERR(path);

			for (char * pCh = path; *pCh; ++pCh)
			{
				if (*pCh < 32 || *pCh > 126)
				{
					WARN("Invalid character %c in path %s", *pCh, path);
					return false;
				}
				else if (*pCh >= 'A' && *pCh <= 'Z')
				{
					*pCh += 32;		// Convert to lowercase
				}
				else if (*pCh == '\\')
				{
					*pCh = '/';		// Normalize paths to forward slashes
				}
			}

			return true;
		}

		// Write a memory buffer out to an asset pack .zip file.
		bool WriteAssetDataToZip(
			const char * assetPath,
			const char * assetSuffix,
			const void * pData,
			size_t sizeBytes,
			mz_zip_archive * pZipOut)
		{
			ASSERT_ERR(assetPath);
			ASSERT_ERR(sizeBytes >= 0);
			ASSERT_ERR(pData || sizeBytes == 0);
			ASSERT_ERR(pZipOut);

			if (!assetSuffix)
				assetSuffix = "";

			// Compose the path, but detect if it's too long
			char zipPath[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE + 1] = {};
			if (_snprintf_s(zipPath, _TRUNCATE, "%s%s", assetPath, assetSuffix) < 0)
			{
				WARN("File path %s%s is too long for .zip format", assetPath, assetSuffix);
				return false;
			}

			CHECK_WARN(NormalizePath(zipPath));

			if (!mz_zip_writer_add_mem(pZipOut, zipPath, pData, sizeBytes, MZ_NO_COMPRESSION))
			{
				WARN("Couldn't add file %s to archive", zipPath);
				return false;
			}

			return true;
		}

		// Parse an asset pack manifest (newline-delimited list of names) into a set structure.
		void ParseManifest(
			const char * manifest,
			int manifestSize,
			const char * path,
			std::unordered_set<std::string> * pManifestOut)
		{
			ASSERT_ERR(manifest);
			ASSERT_ERR(manifestSize > 0);
			ASSERT_ERR(pManifestOut);

			// Make a copy so that we can parse destructively
			std::vector<char> manifestCopy(manifestSize + 1);
			memcpy(&manifestCopy[0], manifest, manifestSize);

			TextParsingHelper tph(&manifestCopy[0], path);
			while (tph.NextLine())
			{
				pManifestOut->insert(std::string(tph.NextToken()));
				tph.ExpectEOL();
			}
		}

		// Compile an entire asset pack from scratch, to a .zip file on disk.
		bool CompileFullAssetPackToFile(
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

			bool success = CompileFullAssetPackToZip(assets, numAssets, &zip);

			if (!mz_zip_writer_finalize_archive(&zip))
			{
				WARN("Couldn't finalize archive");
				success = false;
			}

			mz_zip_writer_end(&zip);
			return success;
		}

		// Compile an entire asset pack from scratch, to a zip stream (can be in memory or a file).
		bool CompileFullAssetPackToZip(
			const AssetCompileInfo * assets,
			int numAssets,
			mz_zip_archive * pZipOut)
		{
			ASSERT_ERR(assets);
			ASSERT_ERR(numAssets > 0);
			ASSERT_ERR(pZipOut);

			// !!!UNDONE: not nicely generating entries in the .zip for directories in the internal paths.
			// Doesn't seem to matter as .zip viewers handle it fine, but maybe we should do that anyway?

			std::string manifest;

			int numErrors = 0;
			for (int iAsset = 0; iAsset < numAssets; ++iAsset)
			{
				const AssetCompileInfo * pACI = &assets[iAsset];
				ACK ack = pACI->m_ack;
				ASSERT_ERR(ack >= 0 && ack < ACK_Count);
			
				LOG("[%d/%d] Compiling %s asset %s...", iAsset+1, numAssets, s_ackNames[ack], pACI->m_pathSrc);

				// Compile the asset
				if (s_assetCompileFuncs[ack](pACI, pZipOut))
				{
					// Write asset name to the manifest
					manifest += pACI->m_pathSrc;
					manifest += '\n';
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
				MTLVER_Current,
				TEXVER_Current,
			};
			if (!WriteAssetDataToZip(s_pathVersionInfo, nullptr, &version, sizeof(version), pZipOut))
				return false;

			// Write manifest
			if (!WriteAssetDataToZip(s_pathManifest, nullptr, &manifest[0], manifest.length(), pZipOut))
				return false;

			return (numErrors == 0);
		}

		// Check if any assets in a pack are out of date by version number or mod time,
		// returning a list of ones that need updating.
		bool FindOutOfDateAssets(
			const char * packPath,
			const AssetCompileInfo * assets,
			int numAssets,
			std::vector<int> * pAssetsToUpdateOut)
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
				pAssetsToUpdateOut->resize(numAssets);
				for (int i = 0; i < numAssets; ++i)
					(*pAssetsToUpdateOut)[i] = i;
				mz_zip_reader_end(&zip);
				return true;
			}

			// Extract the manifest
			fileIndex = mz_zip_reader_locate_file(&zip, s_pathManifest, nullptr, 0);
			if (fileIndex < 0)
			{
				WARN("Couldn't find manifest in asset pack %s", packPath);
				mz_zip_reader_end(&zip);
				return false;
			}
			size_t manifestSize;
			char * pManifest = (char *)mz_zip_reader_extract_to_heap(&zip, fileIndex, &manifestSize, 0);
			if (!pManifest)
			{
				WARN("Couldn't extract manifest from asset pack %s", packPath);
				mz_zip_reader_end(&zip);
				return false;
			}
			std::unordered_set<std::string> manifest;
			ParseManifest(pManifest, int(manifestSize), packPath, &manifest);
			mz_free(pManifest);

			mz_zip_reader_end(&zip);

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
						pAssetsToUpdateOut->push_back(i);
						continue;
					}
					break;

				case ACK_OBJMtlLib:
					if (ver.m_mtlver != MTLVER_Current)
					{
						pAssetsToUpdateOut->push_back(i);
						continue;
					}
					break;

				case ACK_TextureRaw:
				case ACK_TextureWithMips:
					if (ver.m_texver != TEXVER_Current)
					{
						pAssetsToUpdateOut->push_back(i);
						continue;
					}
					break;

				default:
					ERR("Missing case for ACK %d", pACI->m_ack);
					break;
				}

				// Check if the asset exists in the manifest.  If it doesn't, needs to be compiled.
				if (manifest.find(std::string(pACI->m_pathSrc)) == manifest.end())
				{
					pAssetsToUpdateOut->push_back(i);
					continue;
				}

				// Check mod time of the source file against that of the pack
				// If the source file doesn't exist, that's OK!  Asset packs can be
				// distributed in lieu of source files.
				struct _stat srcStat;
				if (_stat(pACI->m_pathSrc, &srcStat) == 0 &&
					srcStat.st_mtime > packStat.st_mtime)
				{
					pAssetsToUpdateOut->push_back(i);
					continue;
				}
			}

			return true;
		}

		// Update an asset pack in-place by recompiling some assets,
		// preserving any other data already in the pack for the others.
		bool UpdateAssetPack(
			const char * packPath,
			const AssetCompileInfo * assets,
			int numAssets,
			std::vector<int> const & assetsToUpdate)
		{
			ASSERT_ERR(packPath);
			ASSERT_ERR(assets);
			ASSERT_ERR(numAssets > 0);
		
			// Load the archive directory
			mz_zip_archive zipSrc = {};
			if (!mz_zip_reader_init_file(&zipSrc, packPath, 0))
			{
				WARN("Couldn't load asset pack %s", packPath);
				return false;
			}
			int numSrcFiles = int(mz_zip_reader_get_num_files(&zipSrc));

			// Generate a temporary filename for the new archive
			char outDir[MAX_PATH] = {};
			if (const char * pLastSlash = max(strrchr(packPath, '/'), strrchr(packPath, '\\')))
			{
				ASSERT_ERR(pLastSlash - packPath < MAX_PATH);
				memcpy(outDir, packPath, pLastSlash - packPath);
			}
			else
			{
				outDir[0] = '.';
			}
			char tempPath[MAX_PATH];
			CHECK_ERR(GetTempFileName(outDir, nullptr, 0, tempPath) != 0);

			// Open the temporary file for writing
			mz_zip_archive zipDest = {};
			if (!mz_zip_writer_init_file(&zipDest, tempPath, 0))
			{
				WARN("Couldn't open temporary file %s for writing", packPath);
				mz_zip_reader_end(&zipSrc);
				return false;
			}

			std::string manifest;
			int numErrors = 0;
			int numAssetsToUpdate = int(assetsToUpdate.size());

			// Iterate over assets, tracking position in both original asset list and
			// list of assets that need updates (a sorted subset of the original ones)
			for (int iAsset = 0, iAssetToUpdate = 0; iAsset < numAssets; ++iAsset)
			{
				const AssetCompileInfo * pACI = &assets[iAsset];

				// Does this asset need recompiling?
				while (iAssetToUpdate < numAssetsToUpdate && assetsToUpdate[iAssetToUpdate] < iAsset)
					++iAssetToUpdate;

				if (iAssetToUpdate < numAssetsToUpdate && assetsToUpdate[iAssetToUpdate] == iAsset)
				{
					ACK ack = pACI->m_ack;
					ASSERT_ERR(ack >= 0 && ack < ACK_Count);
			
					LOG("[%d/%d] Compiling %s asset %s...",
						iAssetToUpdate+1, numAssetsToUpdate, s_ackNames[ack], pACI->m_pathSrc);

					// Compile the asset
					if (s_assetCompileFuncs[ack](pACI, &zipDest))
					{
						// Write asset name to the manifest
						manifest += pACI->m_pathSrc;
						manifest += '\n';
					}
					else
					{
						WARN("Couldn't compile asset %s", pACI->m_pathSrc);
						++numErrors;
					}
				}
				else
				{
					// Copy any files prefixed with the asset name from the old zip to the new one
					// Note, this could be more efficient when there's a large number of files
					for (int i = 0; i < numSrcFiles; ++i)
					{
						char filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
						mz_zip_reader_get_filename(&zipSrc, i, filename, sizeof(filename));
						if (_strnicmp(filename, pACI->m_pathSrc, strlen(pACI->m_pathSrc)) == 0)
						{
							if (!mz_zip_writer_add_from_zip_reader(&zipDest, &zipSrc, i))
							{
								WARN("Couldn't copy file %s from asset pack %s to temporary archive %s",
									filename, packPath, tempPath);
								mz_zip_reader_end(&zipSrc);
								mz_zip_writer_end(&zipDest);
								DeleteFile(tempPath);
								return false;
							}
						}
					}

					// Write asset name to the manifest
					manifest += pACI->m_pathSrc;
					manifest += '\n';
				}
			}

			mz_zip_reader_end(&zipSrc);

			if (numErrors > 0)
			{
				WARN("Failed to compile %d of %d assets", numErrors, numAssetsToUpdate);
			}

			// Write version info
			VersionInfo version =
			{
				PACKVER_Current,
				MESHVER_Current,
				MTLVER_Current,
				TEXVER_Current,
			};
			if (!WriteAssetDataToZip(s_pathVersionInfo, nullptr, &version, sizeof(version), &zipDest))
			{
				mz_zip_writer_end(&zipDest);
				DeleteFile(tempPath);
				return false;
			}

			// Write manifest
			if (!WriteAssetDataToZip(s_pathManifest, nullptr, &manifest[0], manifest.length(), &zipDest))
			{
				mz_zip_writer_end(&zipDest);
				DeleteFile(tempPath);
				return false;
			}

			if (!mz_zip_writer_finalize_archive(&zipDest))
			{
				WARN("Couldn't finalize temporary archive %s", tempPath);
				mz_zip_writer_end(&zipDest);
				DeleteFile(tempPath);
				return false;
			}

			mz_zip_writer_end(&zipDest);

			// Move the new version of the asset pack over the old one
			if (!MoveFileEx(tempPath, packPath, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
			{
				WARN("Couldn't rename temporary file %s over asset pack %s", tempPath, packPath);
				return false;
			}

			return (numErrors == 0);
		}
	}
}
