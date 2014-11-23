#include "framework.h"
#include "miniz.h"

namespace Framework
{
	// Infrastructure for compiling Wavefront .obj files to vertex/index buffers.
	//  * Currently uses hard-coded Vertex structure.
	//  * Creates a single vertex buffer and index buffer, plus a material map that
	//      identifies which faces get drawn with each material.
	//  * Deduplicates verts.
	//  * Generates normals if necessary.
	//  * Groups together all faces with the same material into a contiguous
	//      range of indices so they can be drawn with one draw call.
	//  * !!!UNDONE: Vertex cache optimization.
	//  * !!!UNDONE: Parsing the accompanying .mtl file.

	namespace OBJMeshCompiler
	{
		struct MtlRange
		{
			std::string		m_mtlName;
			int				m_iIdxStart, m_iIdxEnd;
		};

		struct Context
		{
			std::vector<Vertex>		m_verts;
			std::vector<int>		m_indices;
			std::vector<MtlRange>	m_mtlRanges;
			box3					m_bounds;
			bool					m_hasNormals;
		};

		// Prototype various helper functions
		static bool ParseOBJ(const char * path, Context * pCtxOut);
		static void DeduplicateVerts(Context * pCtx);
		static void CalculateNormals(Context * pCtx);
#if VERTEX_TANGENT
		static void CalculateTangents(Context * pCtx);
#endif
	}

	// Compiler entry point

	bool CompileOBJMeshAsset(
		const AssetCompileInfo * pACI,
		mz_zip_archive * pZipOut)
	{
		ASSERT_ERR(pACI);
		ASSERT_ERR(pACI->m_pathSrc);
		ASSERT_ERR(pACI->m_ack == ACK_OBJMesh);
		ASSERT_ERR(pZipOut);

		LOG("Compiling OBJ mesh asset %s...", pACI->m_pathSrc);

		using namespace OBJMeshCompiler;

		Context ctx = {};
		if (!ParseOBJ(pACI->m_pathSrc, &ctx))
			return false;

		DeduplicateVerts(&ctx);
		if (!ctx.m_hasNormals)
			CalculateNormals(&ctx);
#if VERTEX_TANGENT
		CalculateTangents(&ctx);
#endif

		// !!!TEMP
		if (!mz_zip_writer_add_mem(pZipOut, pACI->m_pathSrc, nullptr, 0, MZ_DEFAULT_LEVEL))
		{
			ERR("Couldn't add file %s to archive", pACI->m_pathSrc);
			return false;
		}

		// !!!UNDONE
		return false;
	}



	namespace OBJMeshCompiler
	{
		static bool ParseOBJ(const char * path, Context * pCtxOut)
		{
			ASSERT_ERR(path);
			ASSERT_ERR(pCtxOut);

			// Read the whole file into memory
			std::vector<byte> data;
			if (!LoadFile(path, &data, LFK_Text))
			{
				return false;
			}

			std::vector<point3> positions;
			std::vector<float3> normals;
			std::vector<float2> uvs;

			struct OBJVertex { int iPos, iNormal, iUv; };
			std::vector<OBJVertex> OBJverts;

			struct OBJFace { int iVertStart, iVertEnd, iIdxStart; };
			std::vector<OBJFace> OBJfaces;

			struct OBJMtlRange { std::string mtlName; int iFaceStart, iFaceEnd; };
			std::vector<OBJMtlRange> OBJMtlRanges;
			OBJMtlRange initialRange = { std::string(), 0, 0, };
			OBJMtlRanges.push_back(initialRange);

			// Parse the OBJ format line-by-line
			char * pCtxLine = (char *)&data[0];
			int iLine = 0;
			while (char * pLine = tokenize(pCtxLine, "\n"))
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

				if (_stricmp(pToken, "v") == 0)
				{
					const char * pX = tokenize(pCtxToken, " \t");
					const char * pY = tokenize(pCtxToken, " \t");
					const char * pZ = tokenize(pCtxToken, " \t");
					if (!pX || !pY || !pZ)
						WARN("%s: syntax error at line %d: missing vertex position", path, iLine);

					// Add vertex
					point3 pos;
					pos.x = float(atof(pX));
					pos.y = float(atof(pY));
					pos.z = float(atof(pZ));
					positions.push_back(pos);
				}
				else if (_stricmp(pToken, "vn") == 0)
				{
					const char * pX = tokenize(pCtxToken, " \t");
					const char * pY = tokenize(pCtxToken, " \t");
					const char * pZ = tokenize(pCtxToken, " \t");
					if (!pX || !pY || !pZ)
						WARN("%s: syntax error at line %d: missing normal vector", path, iLine);

					// Add normal
					float3 normal;
					normal.x = float(atof(pX));
					normal.y = float(atof(pY));
					normal.z = float(atof(pZ));
					normals.push_back(normal);
				}
				else if (_stricmp(pToken, "vt") == 0)
				{
					const char * pU = tokenize(pCtxToken, " \t");
					const char * pV = tokenize(pCtxToken, " \t");
					if (!pU || !pV)
						WARN("%s: syntax error at line %d: missing UV", path, iLine);

					// Add UV, flipping V-axis since OBJ UVs use a bottom-up convention
					float2 uv;
					uv.x = float(atof(pU));
					uv.y = 1.0f - float(atof(pV));
					uvs.push_back(uv);
				}
				else if (_stricmp(pToken, "f") == 0)
				{
					// Add face
					OBJFace face = {};
					face.iVertStart = int(OBJverts.size());

					while (char * pCtxVert = tokenize(pCtxToken, " \t"))
					{
						// Parse vertex specification, with slashes separating position, UV, normal indices
						// Note that some components may be missing and will be set to zero here
						OBJVertex vert = {};

						char * pIdx = pCtxVert;
						while (*pCtxVert && *pCtxVert != '/')
							++pCtxVert;
						if (*pCtxVert)
							*(pCtxVert++) = 0;
						vert.iPos = atoi(pIdx);

						pIdx = pCtxVert;
						while (*pCtxVert && *pCtxVert != '/')
							++pCtxVert;
						if (*pCtxVert)
							*(pCtxVert++) = 0;
						vert.iUv = atoi(pIdx);

						vert.iNormal = atoi(pCtxVert);
				
						OBJverts.push_back(vert);
					}

					face.iVertEnd = int(OBJverts.size());

					if (face.iVertEnd == face.iVertStart)
					{
						WARN("%s: syntax error at line %d: missing faces", path, iLine);
						continue;
					}

					OBJfaces.push_back(face);
				}
				else if (_stricmp(pToken, "usemtl") == 0)
				{
					const char * pMtlName = tokenize(pCtxToken, " \t");
					if (!pMtlName)
					{
						WARN("%s: syntax error at line %d: missing material name", path, iLine);
						continue;
					}

					// Close the previous range
					OBJMtlRange * pRange = &OBJMtlRanges.back();
					pRange->iFaceEnd = int(OBJfaces.size());

					// Start a new range if the previous one was nonempty, else overwrite the previous one
					if (pRange->iFaceEnd > pRange->iFaceStart)
					{
						OBJMtlRanges.push_back(OBJMtlRange());
						pRange = &OBJMtlRanges.back();
					}

					// Start the new range
					pRange->mtlName = pMtlName;
					pRange->iFaceStart = int(OBJfaces.size());
				}
				else
				{
					// Unknown command; just ignore
				}
			}

			// Close the last material range
			OBJMtlRanges.back().iFaceEnd = int(OBJfaces.size());

			// Convert OBJ verts to vertex buffer
			pCtxOut->m_verts.reserve(OBJverts.size());
			for (int iVert = 0, cVert = int(OBJverts.size()); iVert < cVert; ++iVert)
			{
				OBJVertex objv = OBJverts[iVert];
				Vertex v = {};

				// OBJ indices are 1-based; fix that (missing components are zeros)
				if (objv.iPos > 0)
					v.m_pos = positions[objv.iPos - 1];
				if (objv.iNormal > 0)
					v.m_normal = normals[objv.iNormal - 1];
				if (objv.iUv > 0)
					v.m_uv = uvs[objv.iUv - 1];

				pCtxOut->m_verts.push_back(v);
			}

			// Convert OBJ faces to index buffer
			for (int iFace = 0, cFace = int(OBJfaces.size()); iFace < cFace; ++iFace)
			{
				OBJFace & face = OBJfaces[iFace];

				// Store where the face ended up in the buffer
				face.iIdxStart = int(pCtxOut->m_indices.size());

				int iVertBase = face.iVertStart;

				// Triangulate the face
				for (int iVert = face.iVertStart + 2; iVert < face.iVertEnd; ++iVert)
				{
					pCtxOut->m_indices.push_back(iVertBase);
					pCtxOut->m_indices.push_back(iVert - 1);
					pCtxOut->m_indices.push_back(iVert);
				}
			}

			// Add one more OBJface as a sentinel for the next part
			OBJFace faceSentinel = { 0, 0, int(pCtxOut->m_indices.size()) };
			OBJfaces.push_back(faceSentinel);

			// Convert OBJ material ranges (in terms of faces) to ranges in terms of indices
			for (int iRange = 0, cRange = int(OBJMtlRanges.size()); iRange < cRange; ++iRange)
			{
				OBJMtlRange & objrange = OBJMtlRanges[iRange];
				MtlRange range =
				{
					objrange.mtlName,
					OBJfaces[objrange.iFaceStart].iIdxStart,
					OBJfaces[objrange.iFaceEnd].iIdxStart,
				};
				pCtxOut->m_mtlRanges.push_back(range);
			}

			pCtxOut->m_bounds = makebox3(int(positions.size()), &positions[0]);
			pCtxOut->m_hasNormals = !normals.empty();

			return true;
		}

		static void DeduplicateVerts(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// !!!UNDONE
		}

		static void CalculateNormals(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// !!!UNDONE
		}

#if VERTEX_TANGENT
		static void CalculateTangents(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// !!!UNDONE
		}
#endif
	}
}
