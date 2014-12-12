#pragma once

namespace Framework
{
	class Texture2D;
	class TextureLib;

	// Very simple, hard-coded set of parameters for now
	struct Material
	{
		const char *	m_mtlName;
		Texture2D *		m_pTexDiffuseColor;
		Texture2D *		m_pTexSpecColor;
		Texture2D *		m_pTexHeight;
		rgb				m_rgbDiffuseColor;
		rgb				m_rgbSpecColor;
		float			m_specPower;
	};

	class MaterialLib
	{
	public:
		// Asset pack that the material data is sourced from
		comptr<AssetPack>			m_pPack;

		// Table of materials by name
		std::unordered_map<std::string, Material>	m_mtls;

					MaterialLib();
		Material *	Lookup(const char * name);
		void		Reset();
	};

	// Load a material library from an asset pack and resolve texture
	// references using the given texture library
	bool LoadMaterialLibFromAssetPack(
		AssetPack * pPack,
		const char * path,
		TextureLib * pTexLib,
		MaterialLib * pMtlLibOut);
}
