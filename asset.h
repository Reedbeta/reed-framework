#pragma once

namespace Framework
{
	class AssetPack : public RefCount
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
		std::unordered_set<std::string>			m_manifest;			// List of asset names in the pack
		std::string								m_path;				// File path where the asset pack was loaded from

		AssetPack();
		bool LookupFile(const char * path, const char * suffix, void ** pDataOut, int * pSizeOut);
		bool HasAsset(const char * path);
		void Reset();
	};

	enum ACK					// Asset Compile Kind
	{
		ACK_OBJMesh,			// .obj mesh, compiled to vtx/idx buffers and mtl map
		ACK_OBJMtlLib,			// .mtl material library that goes alongside an .obj
		ACK_TextureRaw,			// Single RGBA8 image
		ACK_TextureWithMips,	// RGBA8 image, resampled up to pow2 and mips generated

		ACK_Count
	};

	struct AssetCompileInfo
	{
		const char *	m_pathSrc;
		ACK				m_ack;
	};

	// Load an asset pack file, checking that all its assets are present and up to date,
	// and compiling any that aren't.
	bool LoadAssetPackOrCompileIfOutOfDate(
		const char * packPath,
		const AssetCompileInfo * assets,
		int numAssets,
		AssetPack * pPackOut);

	// Just load an asset pack file.
	bool LoadAssetPack(
		const char * packPath,
		AssetPack * pPackOut);
}
