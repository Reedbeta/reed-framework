#pragma once

namespace Framework
{
	class AssetPack
	{
	public:
		struct FileInfo
		{
			std::string		m_path;			// Archive internal path
			int				m_offset;		// Starting offset into m_data
			int				m_size;			// Size in bytes
		};

		std::vector<byte>						m_data;				// Entire uncompressed archive
		std::vector<FileInfo>					m_files;			// List of files in the archive
		std::unordered_map<std::string, int>	m_directory;		// Mapping from internal path to index in m_files

		AssetPack();
		byte * LookupFile(const char * path);
		void Release();
	};

	// Load an asset pack file, checking that all its assets are present and up to date,
	// and compiling any that aren't.
	bool LoadAssetPackOrCompileIfOutOfDate(
		const char * packPath,
		const char ** assetPaths,
		int numAssets,
		AssetPack * pPackOut);

	// Just load an asset pack file, THAT'S ALL.
	bool LoadAssetPack(
		const char * packPath,
		AssetPack * pPackOut);

	// Compile an entire asset pack from scratch.
	bool CompileFullAssetPack(
		const char * packPath,
		const char ** assetPaths,
		int numAssets);

	// Update an asset pack by compiling the assets that are out of date or missing from it.
	bool UpdateAssetPack(
		const char * packPath,
		const char ** assetPaths,
		int numAssets);
}
