#include "framework.h"
#include "miniz.h"

namespace Framework
{
	// Infrastructure for compiling Wavefront .mtl material libraries.

	namespace OBJMtlLibCompiler
	{
		static const char * s_suffixMtlLib = "/material_library";

		struct Material
		{
			std::string		m_mtlName;
			std::string		m_texDiffuseColor;
			std::string		m_texSpecColor;
			std::string		m_texHeight;
			srgb			m_rgbDiffuseColor;
			srgb			m_rgbSpecColor;
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
			char * pCtxLine = (char *)&data[0];
			int iLine = 0;
			while (char * pLine = tokenizeConsecutive(pCtxLine, "\n"))
			{
				++iLine;

				// Strip comments starting with #
				if (char * pChzComment = strchr(pLine, '#'))
					*pChzComment = 0;

				// Parse the line token-by-token
				char * pCtxToken = pLine;
				char * pToken = tokenize(pCtxToken, " \t");

				// Ignore blank lines
				if (!pToken)
					continue;

				if (_stricmp(pToken, "newmtl") == 0)
				{
					const char * pMtlName = tokenize(pCtxToken, " \t");
					if (!pMtlName)
						WARN("%s: syntax error at line %d: missing material name", path, iLine);
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

					pCtxOut->m_mtls.push_back(Material());
					pMtlCur = &pCtxOut->m_mtls.back();
					pMtlCur->m_mtlName = pMtlName;
				}
				else if (_stricmp(pToken, "map_Kd") == 0)
				{
					if (!pMtlCur)
					{
						WARN("%s: syntax error at line %d: material parameters specified before any \"newmtl\" command; ignoring",
							path, iLine);
						continue;
					}

					const char * pTexName = tokenize(pCtxToken, " \t");
					if (!pTexName)
						WARN("%s: syntax error at line %d: missing texture name", path, iLine);
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

					pMtlCur->m_texDiffuseColor = pTexName;
				}
				else if (_stricmp(pToken, "map_Ks") == 0)
				{
					if (!pMtlCur)
					{
						WARN("%s: syntax error at line %d: material parameters specified before any \"newmtl\" command; ignoring",
							path, iLine);
						continue;
					}

					const char * pTexName = tokenize(pCtxToken, " \t");
					if (!pTexName)
						WARN("%s: syntax error at line %d: missing texture name", path, iLine);
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

					pMtlCur->m_texSpecColor = pTexName;
				}
				else if (_stricmp(pToken, "map_bump") == 0 ||
						 _stricmp(pToken, "bump") == 0)
				{
					if (!pMtlCur)
					{
						WARN("%s: syntax error at line %d: material parameters specified before any \"newmtl\" command; ignoring",
							path, iLine);
						continue;
					}

					const char * pTexName = tokenize(pCtxToken, " \t");
					if (!pTexName)
						WARN("%s: syntax error at line %d: missing texture name", path, iLine);
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

					pMtlCur->m_texHeight = pTexName;
				}
				else if (_stricmp(pToken, "Kd") == 0)
				{
					const char * pR = tokenize(pCtxToken, " \t");
					const char * pG = tokenize(pCtxToken, " \t");
					const char * pB = tokenize(pCtxToken, " \t");
					if (!pR || !pG || !pB)
						WARN("%s: syntax error at line %d: missing RGB color", path, iLine);
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

					srgb color = makesrgb(float(atof(pR)), float(atof(pG)), float(atof(pB)));
					if (any(color < 0.0f) || any(color > 1.0f))
					{
						WARN("%s: RGB color at line %d is outside [0, 1]; clamping", path, iLine);
						color = saturate(color);
					}
					pMtlCur->m_rgbDiffuseColor = color;
				}
				else if (_stricmp(pToken, "Ks") == 0)
				{
					const char * pR = tokenize(pCtxToken, " \t");
					const char * pG = tokenize(pCtxToken, " \t");
					const char * pB = tokenize(pCtxToken, " \t");
					if (!pR || !pG || !pB)
						WARN("%s: syntax error at line %d: missing RGB color", path, iLine);
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

					srgb color = makesrgb(float(atof(pR)), float(atof(pG)), float(atof(pB)));
					if (any(color < 0.0f) || any(color > 1.0f))
					{
						WARN("%s: RGB color at line %d is outside [0, 1]; clamping", path, iLine);
						color = saturate(color);
					}
					pMtlCur->m_rgbSpecColor = color;
				}
				else if (_stricmp(pToken, "Ns") == 0)
				{
					const char * pN = tokenize(pCtxToken, " \t");
					if (!pN)
						WARN("%s: syntax error at line %d: missing number", path, iLine);
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

					float n = float(atof(pN));
					if (n < 0.0f)
					{
						WARN("%s: specular power at line %d is below zero; clamping", path, iLine);
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
}
