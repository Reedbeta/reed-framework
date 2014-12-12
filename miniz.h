#pragma once
#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

// Convenience functions for the asset compiler/loader

inline bool CheckPathChars(const char * path)
{
	ASSERT_ERR(path);

	// Check that filenames are printable-ASCII-only, lowercase, and there are no backslashes
	// (this should really be generalized to allow UTF-8 printable chars)
	for (const char * pCh = path; *pCh; ++pCh)
	{
		if (*pCh < 32 || (*pCh >= 'A' && *pCh <= 'Z') || *pCh > 126 || *pCh == '\\')
		{
			WARN("Invalid character %c in path %s", *pCh, path);
			return false;
		}
	}

	return true;
}

inline bool WriteAssetDataToZip(
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

	// Compose the path, but detect if it's too long
	if (assetSuffix)
	{
		char zipPath[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE + 1] = {};
		if (_snprintf_s(zipPath, _TRUNCATE, "%s%s", assetPath, assetSuffix) < 0)
		{
			WARN("File path %s%s is too long for .zip format", assetPath, assetSuffix);
			return false;
		}

		CHECK_WARN(CheckPathChars(zipPath));

		if (!mz_zip_writer_add_mem(pZipOut, zipPath, pData, sizeBytes, MZ_DEFAULT_LEVEL))
		{
			WARN("Couldn't add file %s to archive", zipPath);
			return false;
		}
	}
	else
	{
		if (strlen(assetPath) > MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE)
		{
			WARN("File path %s is too long for .zip format", assetPath);
			return false;
		}

		CHECK_WARN(CheckPathChars(assetPath));

		if (!mz_zip_writer_add_mem(pZipOut, assetPath, pData, sizeBytes, MZ_DEFAULT_LEVEL))
		{
			WARN("Couldn't add file %s to archive", assetPath);
			return false;
		}
	}

	return true;
}
