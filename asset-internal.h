#pragma once

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

namespace Framework
{
	// Infrastructure for compiling art source files (such as Wavefront .obj meshes, and
	// textures in .bmp/.psd/whatever) to engine-friendly data (such as vertex/index buffers,
	// and RGBA8 pixel data with pre-generated mipmaps).
	//
	//  * Takes a list of source files to compile.  Current assumption is 1 source file == 1 asset.
	//
	//  * Compiled data is stored as a set of files in a .zip.  The source file path is used
	//      as a directory name in the .zip.  For example, source file "foo/bar/baz.obj" will
	//      result in a directory "foo/bar/baz.obj/" with files in it for verts, indices, etc.
	//
	//  * Compiled data is considered out-of-date and recompiled if the mod time of the source
	//      file is newer than the mod time of the asset pack (the .zip).
	//
	//  * Version numbers for the whole pack system and each asset type are also stored in the
	//      .zip, and mismatches will trigger recompilation.
	//
	//  !!!UNDONE: build the list of sources to compile by following dependencies from some root.

	namespace AssetCompiler
	{
		enum PACKVER
		{
			PACKVER_Current = 3,
		};

		enum MESHVER
		{
			MESHVER_Current = 4,
		};

		enum MTLVER
		{
			MTLVER_Current = 1,
		};

		enum TEXVER
		{
			TEXVER_Current = 1,
		};

		struct VersionInfo
		{
			PACKVER		m_packver;
			MESHVER		m_meshver;
			MTLVER		m_mtlver;
			TEXVER		m_texver;
		};

		// Load an asset pack file from a zip stream (can be in memory or a file).
		bool LoadAssetPackFromZip(
			mz_zip_archive * pZip,
			AssetPack * pPackOut);

		// Check that filenames are printable-ASCII-only, lowercase, and there are no backslashes
		// (this should really be generalized to allow UTF-8 printable chars)
		bool CheckPathChars(const char * path);

		// Write a memory buffer out to an asset pack .zip file.
		bool WriteAssetDataToZip(
			const char * assetPath,
			const char * assetSuffix,
			const void * pData,
			size_t sizeBytes,
			mz_zip_archive * pZipOut);

		// Parse an asset pack manifest (newline-delimited list of names) into a set structure.
		void ParseManifest(
			const char * manifest,
			int manifestSize,
			const char * path,
			std::unordered_set<std::string> * pManifestOut);

		// Compile an entire asset pack from scratch, to a .zip file on disk.
		bool CompileFullAssetPackToFile(
			const char * packPath,
			const AssetCompileInfo * assets,
			int numAssets);

		// Compile an entire asset pack from scratch, to a zip stream (can be in memory or a file).
		bool CompileFullAssetPackToZip(
			const AssetCompileInfo * assets,
			int numAssets,
			mz_zip_archive * pZipOut);

		// Check if any assets in a pack are out of date by version number or mod time,
		// returning a list of ones that need updating (as indices into the assets array).
		bool FindOutOfDateAssets(
			const char * packPath,
			const AssetCompileInfo * assets,
			int numAssets,
			std::vector<int> * pAssetsToUpdateOut);

		// Update an asset pack in-place by recompiling some assets,
		// preserving any other data already in the pack for others.
		bool UpdateAssetPack(
			const char * packPath,
			const AssetCompileInfo * assets,
			int numAssets,
			std::vector<int> const & assetsToUpdate);
	}
}
