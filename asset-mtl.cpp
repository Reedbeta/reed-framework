#include "framework.h"
#include "miniz.h"

namespace Framework
{
	// Infrastructure for compiling Wavefront .mtl material libraries.

	namespace OBJMtlLibCompiler
	{
		static const char * s_suffixMtlLib = "/material_lib";

		struct Material
		{
			std::string		m_mtlName;
			std::string		m_texDiffuseColor;
			std::string		m_texSpecColor;
			std::string		m_texHeight;
			rgb				m_rgbDiffuseColor;
			rgb				m_rgbSpecColor;
			float			m_specPower;
		};

		struct Context
		{
			std::vector<Material>	m_mtls;
		};

		// Prototype various helper functions
		static bool ParseMTL(const char * path, Context * pCtxOut);
		static void SerializeMtlLib(Context * pCtx, std::vector<byte> * pDataOut);
	}



	// Compiler entry point

	bool CompileOBJMtlLibAsset(
		const AssetCompileInfo * pACI,
		mz_zip_archive * pZipOut)
	{
		ASSERT_ERR(pACI);
		ASSERT_ERR(pACI->m_pathSrc);
		ASSERT_ERR(pACI->m_ack == ACK_OBJMtlLib);
		ASSERT_ERR(pZipOut);

		using namespace OBJMtlLibCompiler;

		// Read the material definitions from the MTL file
		Context ctx = {};
		if (!ParseMTL(pACI->m_pathSrc, &ctx))
			return false;

		// Write the data out to the archive

		std::vector<byte> serializedMtlLib;
		SerializeMtlLib(&ctx, &serializedMtlLib);

		return WriteAssetDataToZip(pACI->m_pathSrc, s_suffixMtlLib, &serializedMtlLib[0], serializedMtlLib.size(), pZipOut);
	}



	namespace OBJMtlLibCompiler
	{
		static bool ParseMTL(const char * path, Context * pCtxOut)
		{
			ASSERT_ERR(path);
			ASSERT_ERR(pCtxOut);

			// Read the whole file into memory
			std::vector<byte> data;
			if (!LoadFile(path, &data, LFK_Text))
				return false;

			Material * pMtlCur = nullptr;

			// Parse line-by-line
			TextParsingHelper tph((char *)&data[0]);
			while (tph.NextLine())
			{
				char * pToken = tph.NextToken();
				if (_stricmp(pToken, "newmtl") == 0)
				{
					pCtxOut->m_mtls.push_back(Material());
					pMtlCur = &pCtxOut->m_mtls.back();
					pMtlCur->m_mtlName = tph.ExpectOneToken(path, "material name");
					makeLowercase(pMtlCur->m_mtlName);
				}
				else if (_stricmp(pToken, "map_Kd") == 0)
				{
					if (!pMtlCur)
					{
						WARN("%s: syntax error at line %d: material parameters specified before any \"newmtl\" command; ignoring",
							path, tph.m_iLine);
						continue;
					}

					pMtlCur->m_texDiffuseColor = tph.ExpectOneToken(path, "texture name");
					makeLowercase(pMtlCur->m_texDiffuseColor);
				}
				else if (_stricmp(pToken, "map_Ks") == 0)
				{
					if (!pMtlCur)
					{
						WARN("%s: syntax error at line %d: material parameters specified before any \"newmtl\" command; ignoring",
							path, tph.m_iLine);
						continue;
					}

					pMtlCur->m_texSpecColor = tph.ExpectOneToken(path, "texture name");
					makeLowercase(pMtlCur->m_texSpecColor);
				}
				else if (_stricmp(pToken, "map_bump") == 0 ||
						 _stricmp(pToken, "bump") == 0)
				{
					if (!pMtlCur)
					{
						WARN("%s: syntax error at line %d: material parameters specified before any \"newmtl\" command; ignoring",
							path, tph.m_iLine);
						continue;
					}

					pMtlCur->m_texHeight = tph.ExpectOneToken(path, "texture name");
					makeLowercase(pMtlCur->m_texHeight);
				}
				else if (_stricmp(pToken, "Kd") == 0)
				{
					char * tokens[3] = {};
					tph.ExpectTokens(tokens, dim(tokens), path, "RGB color");

					srgb color = makesrgb(float(atof(tokens[0])), float(atof(tokens[1])), float(atof(tokens[2])));
					if (any(color < 0.0f) || any(color > 1.0f))
					{
						WARN("%s: RGB color at line %d is outside [0, 1]; clamping", path, tph.m_iLine);
						color = saturate(color);
					}
					pMtlCur->m_rgbDiffuseColor = toLinear(color);
				}
				else if (_stricmp(pToken, "Ks") == 0)
				{
					char * tokens[3] = {};
					tph.ExpectTokens(tokens, dim(tokens), path, "RGB color");

					srgb color = makesrgb(float(atof(tokens[0])), float(atof(tokens[1])), float(atof(tokens[2])));
					if (any(color < 0.0f) || any(color > 1.0f))
					{
						WARN("%s: RGB color at line %d is outside [0, 1]; clamping", path, tph.m_iLine);
						color = saturate(color);
					}
					pMtlCur->m_rgbSpecColor = toLinear(color);
				}
				else if (_stricmp(pToken, "Ns") == 0)
				{
					float n = float(atof(tph.ExpectOneToken(path, "specular power")));
					if (n < 0.0f)
					{
						WARN("%s: specular power at line %d is below zero; clamping", path, tph.m_iLine);
						n = 0.0f;
					}
					pMtlCur->m_specPower = n;
				}
				else
				{
					// Unknown command; just ignore
				}
			}

			return true;
		}

		static void SerializeMtlLib(Context * pCtx, std::vector<byte> * pDataOut)
		{
			ASSERT_ERR(pCtx);
			ASSERT_ERR(pDataOut);

			for (int i = 0, cMtl = int(pCtx->m_mtls.size()); i < cMtl; ++i)
			{
				const Material * pMtl = &pCtx->m_mtls[i];

				// Write out all the strings as null-terminated, followed by the colors

				int nameLength = int(pMtl->m_mtlName.size()) + 1;
				int texDiffuseLength = int(pMtl->m_texDiffuseColor.size()) + 1;
				int texSpecLength = int(pMtl->m_texSpecColor.size()) + 1;
				int texHeightLength = int(pMtl->m_texHeight.size()) + 1;
				int totalLength = nameLength + texDiffuseLength + texSpecLength + texHeightLength + 7 * sizeof(float);

				int iByteStart = int(pDataOut->size());
				pDataOut->resize(iByteStart + totalLength);
				byte * pWrite = (byte *)&(*pDataOut)[iByteStart];

				memcpy(pWrite, pMtl->m_mtlName.c_str(), nameLength); pWrite += nameLength;
				memcpy(pWrite, pMtl->m_texDiffuseColor.c_str(), texDiffuseLength); pWrite += texDiffuseLength;
				memcpy(pWrite, pMtl->m_texSpecColor.c_str(), texSpecLength); pWrite += texSpecLength;
				memcpy(pWrite, pMtl->m_texHeight.c_str(), texHeightLength); pWrite += texHeightLength;
				memcpy(pWrite, &pMtl->m_rgbDiffuseColor, 7 * sizeof(float));
			}
		}
	}



	// Load compiled data into a runtime game object

	bool LoadMaterialLibFromAssetPack(
		AssetPack * pPack,
		const char * path,
		TextureLib * pTexLib,
		MaterialLib * pMtlLibOut)
	{
		ASSERT_ERR(pPack);
		ASSERT_ERR(path);
		ASSERT_ERR(pMtlLibOut);

		using namespace OBJMtlLibCompiler;

		pMtlLibOut->m_pPack = pPack;

		// Look for the data in the asset pack
		byte * pData;
		int dataSize;
		if (!pPack->LookupFile(path, s_suffixMtlLib, (void **)&pData, &dataSize))
		{
			WARN("Couldn't find data for material lib %s in asset pack %s", path, pPack->m_path.c_str());
			return false;
		}

		// Deserialize it
		const byte * pCur = pData;
		const byte * pEnd = pData + dataSize;
		while (pCur < pEnd)
		{
			Framework::Material mtl = { (const char *)pCur, };

			while (pCur < pEnd && *pCur)
				++pCur;
			if (pCur == pEnd)
			{
				WARN("Corrupt material lib: unterminated string");
				return false;
			}
			++pCur;

			const char * pTexDiffuseColor = (const char *)pCur;
			while (pCur < pEnd && *pCur)
				++pCur;
			if (pCur == pEnd)
			{
				WARN("Corrupt material lib: unterminated string");
				return false;
			}
			++pCur;

			const char * pTexSpecColor = (const char *)pCur;
			while (pCur < pEnd && *pCur)
				++pCur;
			if (pCur == pEnd)
			{
				WARN("Corrupt material lib: unterminated string");
				return false;
			}
			++pCur;

			const char * pTexHeight = (const char *)pCur;
			while (pCur < pEnd && *pCur)
				++pCur;
			if (pCur == pEnd)
			{
				WARN("Corrupt material lib: unterminated string");
				return false;
			}
			++pCur;

			memcpy(&mtl.m_rgbDiffuseColor, pCur, 7 * sizeof(float));
			pCur += 7 * sizeof(float);
			if (any(mtl.m_rgbDiffuseColor < 0.0f) || any(mtl.m_rgbDiffuseColor > 1.0f) ||
				any(mtl.m_rgbSpecColor < 0.0f) || any(mtl.m_rgbSpecColor > 1.0f) ||
				mtl.m_specPower < 0.0f)
			{
				WARN("Corrupt material lib: invalid parameter data");
				return false;
			}

			// Look up textures by name
			if (pTexLib)
			{
				if (*pTexDiffuseColor)
				{
					mtl.m_pTexDiffuseColor = pTexLib->Lookup(pTexDiffuseColor);
					ASSERT_WARN_MSG(mtl.m_pTexDiffuseColor, 
						"Material %s: couldn't find texture %s in texture library", mtl.m_mtlName, pTexDiffuseColor);
				}
				if (*pTexSpecColor)
				{
					mtl.m_pTexSpecColor = pTexLib->Lookup(pTexSpecColor);
					ASSERT_WARN_MSG(mtl.m_pTexSpecColor, 
						"Material %s: couldn't find texture %s in texture library", mtl.m_mtlName, pTexSpecColor);
				}
				if (*pTexHeight)
				{
					mtl.m_pTexHeight = pTexLib->Lookup(pTexHeight);
					ASSERT_WARN_MSG(mtl.m_pTexHeight, 
						"Material %s: couldn't find texture %s in texture library", mtl.m_mtlName, pTexHeight);
				}
			}

			pMtlLibOut->m_mtls.insert(std::make_pair(std::string(mtl.m_mtlName), mtl));
		}

		return true;
	}
}
