#pragma once
#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

// Convenience functions for the asset compiler/loader

inline bool WriteAssetDataToZip(
	const char * assetPath,
	const char * assetSuffix,
	const void * pData,
	size_t sizeBytes,
	mz_zip_archive * pZipOut)
{
	// Compose the path, but detect if it's too long
	char zipPath[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE + 1] = {};
	if (_snprintf_s(zipPath, _TRUNCATE, "%s%s", assetPath, assetSuffix) < 0)
	{
		ERR("File path %s%s is too long for .zip format", assetPath, assetSuffix);
		return false;
	}

	if (!mz_zip_writer_add_mem(pZipOut, zipPath, pData, sizeBytes, MZ_DEFAULT_LEVEL))
	{
		ERR("Couldn't add file %s to archive", zipPath);
		return false;
	}

	return true;
}
