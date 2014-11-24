#include "framework.h"
#include "miniz.h"
#include <algorithm>

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

	static const char * s_suffixVerts =		"/verts";
	static const char * s_suffixIndices =	"/indices";
	static const char * s_suffixMtlMap =	"/mtlmap";
	static const char * s_suffixBounds =	"/bounds";

	namespace OBJMeshCompiler
	{
		struct MtlRange
		{
			std::string		m_mtlName;
			int				m_iIdxStart, m_cIdx;
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
		static void SortMaterials(Context * pCtx);

		static void SerializeMaterialMap(Context * pCtx, std::vector<byte> * pDataOut);
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

		// Read the mesh data from the OBJ file
		Context ctx = {};
		if (!ParseOBJ(pACI->m_pathSrc, &ctx))
			return false;

		// Clean up the mesh
		DeduplicateVerts(&ctx);
		if (!ctx.m_hasNormals)
			CalculateNormals(&ctx);
#if VERTEX_TANGENT
		CalculateTangents(&ctx);
#endif
		SortMaterials(&ctx);

		// Write the data out to the archive

		std::vector<byte> serializedMaterialMap;
		SerializeMaterialMap(&ctx, &serializedMaterialMap);

		if (!WriteAssetDataToZip(pACI->m_pathSrc, s_suffixVerts, &ctx.m_verts[0], ctx.m_verts.size() * sizeof(Vertex), pZipOut) ||
			!WriteAssetDataToZip(pACI->m_pathSrc, s_suffixIndices, &ctx.m_indices[0], ctx.m_indices.size() * sizeof(int), pZipOut) ||
			!WriteAssetDataToZip(pACI->m_pathSrc, s_suffixMtlMap, &serializedMaterialMap[0], serializedMaterialMap.size(), pZipOut) ||
			!WriteAssetDataToZip(pACI->m_pathSrc, s_suffixBounds, &ctx.m_bounds, sizeof(ctx.m_bounds), pZipOut))
		{
			return false;
		}

		return true;
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

				if (_stricmp(pToken, "v") == 0)
				{
					const char * pX = tokenize(pCtxToken, " \t");
					const char * pY = tokenize(pCtxToken, " \t");
					const char * pZ = tokenize(pCtxToken, " \t");
					if (!pX || !pY || !pZ)
						WARN("%s: syntax error at line %d: missing vertex position", path, iLine);
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

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
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

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
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

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
					if (const char * pExtra = tokenize(pCtxToken, " \t"))
						WARN("%s: syntax error at line %d: unexpected extra token \"%s\"; ignoring", path, iLine, pExtra);

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
				int iIdxStart = OBJfaces[objrange.iFaceStart].iIdxStart;
				int iIdxEnd = OBJfaces[objrange.iFaceEnd].iIdxStart;
				MtlRange range = { objrange.mtlName, iIdxStart, iIdxEnd - iIdxStart, };
				pCtxOut->m_mtlRanges.push_back(range);
			}

			pCtxOut->m_bounds = makebox3(int(positions.size()), &positions[0]);
			pCtxOut->m_hasNormals = !normals.empty();

			return true;
		}

		static void DeduplicateVerts(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			struct VertexHasher
			{
				std::hash<float> fh;
				size_t operator () (const Vertex & v) const
				{
					return fh(v.m_pos.x) ^ fh(v.m_pos.y) ^ fh(v.m_pos.z) ^
						   fh(v.m_normal.x) ^ fh(v.m_normal.y) ^ fh(v.m_normal.z) ^
						   fh(v.m_uv.x) ^ fh(v.m_uv.y);
				}
			};

			struct VertexEqualityTester
			{
				bool operator () (const Vertex & u, const Vertex & v) const
				{
					return (all(u.m_pos == v.m_pos) &&
							all(u.m_normal == v.m_normal) &&
							all(u.m_uv == v.m_uv));
				}
			};

			std::vector<Vertex> vertsDeduplicated;
			std::vector<int> remappingTable;
			std::unordered_map<Vertex, int, VertexHasher, VertexEqualityTester> mapVertToIndex;

			vertsDeduplicated.reserve(pCtx->m_verts.size());
			remappingTable.reserve(pCtx->m_verts.size());

			for (int i = 0, cVert = int(pCtx->m_verts.size()); i < cVert; ++i)
			{
				const Vertex & vert = pCtx->m_verts[i];
				auto iter = mapVertToIndex.find(vert);
				if (iter == mapVertToIndex.end())
				{
					// Found a new vertex that's not in the map yet.
					int newIndex = int(vertsDeduplicated.size());
					vertsDeduplicated.push_back(vert);
					remappingTable.push_back(newIndex);
					mapVertToIndex.insert(std::make_pair(vert, newIndex));
				}
				else
				{
					// It's already in the map; re-use the previous index
					int newIndex = iter->second;
					remappingTable.push_back(newIndex);
				}
			}

			ASSERT_ERR(vertsDeduplicated.size() <= pCtx->m_verts.size());
			ASSERT_ERR(remappingTable.size() == pCtx->m_verts.size());

			std::vector<int> indicesRemapped;
			indicesRemapped.resize(pCtx->m_indices.size());

			for (int i = 0, cIndex = int(pCtx->m_indices.size()); i < cIndex; ++i)
			{
				indicesRemapped[i] = remappingTable[pCtx->m_indices[i]];
			}

			pCtx->m_verts.swap(vertsDeduplicated);
			pCtx->m_indices.swap(indicesRemapped);
		}

		static void CalculateNormals(Context * pCtx)
		{
			ASSERT_ERR(pCtx);
			ASSERT_WARN(pCtx->m_indices.size() % 3 == 0);

			// Generate a normal for each triangle, and accumulate onto vertex
			for (int i = 0, c = int(pCtx->m_indices.size()); i < c; i += 3)
			{
				int indices[3] = { pCtx->m_indices[i], pCtx->m_indices[i+1], pCtx->m_indices[i+2] };

				// Gather positions for this triangle
				point3 facePositions[3] =
				{
					pCtx->m_verts[indices[0]].m_pos,
					pCtx->m_verts[indices[1]].m_pos,
					pCtx->m_verts[indices[2]].m_pos,
				};

				// Calculate edge and normal vectors
				float3 edge0 = facePositions[1] - facePositions[0];
				float3 edge1 = facePositions[2] - facePositions[0];
				float3 normal = normalize(cross(edge0, edge1));

				// Accumulate onto vertices
				pCtx->m_verts[indices[0]].m_normal += normal;
				pCtx->m_verts[indices[1]].m_normal += normal;
				pCtx->m_verts[indices[2]].m_normal += normal;
			}

			// Normalize summed normals
			for (int i = 0, c = int(pCtx->m_verts.size()); i < c; ++i)
			{
				pCtx->m_verts[i].m_normal = normalize(pCtx->m_verts[i].m_normal);
				ASSERT_WARN(all(isfinite(pCtx->m_verts[i].m_normal)));
			}
		}

#if VERTEX_TANGENT
		static void CalculateTangents(Context * pCtx)
		{
			ASSERT_ERR(pCtx);
			ASSERT_WARN(pCtx->m_indices.size() % 3 == 0);

			// Generate a tangent for each triangle, based on triangle's UV mapping,
			// and accumulate onto vertex
			for (int i = 0, c = int(pCtx->m_indices.size()); i < c; i += 3)
			{
				int indices[3] = { pCtx->m_indices[i], pCtx->m_indices[i+1], pCtx->m_indices[i+2] };

				// Gather positions for this triangle
				point3 facePositions[3] =
				{
					pCtx->m_verts[indices[0]].m_pos,
					pCtx->m_verts[indices[1]].m_pos,
					pCtx->m_verts[indices[2]].m_pos,
				};

				// Calculate edge and normal vectors
				float3 edge0 = facePositions[1] - facePositions[0];
				float3 edge1 = facePositions[2] - facePositions[0];
				float3 normal = cross(edge0, edge1);

				// Calculate matrix from unit triangle to position space
				float3x3 matUnitToPosition = makefloat3x3(edge0, edge1, normal);

				// Gather UVs for this triangle
				float2 faceUVs[3] =
				{
					pCtx->m_verts[indices[0]].m_uv,
					pCtx->m_verts[indices[1]].m_uv,
					pCtx->m_verts[indices[2]].m_uv,
				};

				// Calculate UV space edge vectors
				float2 uvEdge0 = faceUVs[1] - faceUVs[0];
				float2 uvEdge1 = faceUVs[2] - faceUVs[0];

				// Calculate matrix from unit triangle to UV space
				float3x3 matUnitToUV = float3x3::identity();
				matUnitToUV[0].xy = uvEdge0;
				matUnitToUV[1].xy = uvEdge1;

				// Calculate matrix from UV space to position space
				float3x3 matUVToPosition = inverse(matUnitToUV) * matUnitToPosition;

				// The x-axis of that matrix is the tangent vector
				float3 tangent = normalize(matUVToPosition[0]);

				// Accumulate onto vertices
				pCtx->m_verts[indices[0]].m_tangent += tangent;
				pCtx->m_verts[indices[1]].m_tangent += tangent;
				pCtx->m_verts[indices[2]].m_tangent += tangent;
			}

			// Normalize summed tangents
			for (int i = 0, c = int(pCtx->m_verts.size()); i < c; ++i)
			{
				pCtx->m_verts[i].m_tangent = normalize(pCtx->m_verts[i].m_tangent);
				ASSERT_WARN(all(isfinite(pCtx->m_verts[i].m_tangent)));
			}
		}
#endif // VERTEX_TANGENT

		static void SortMaterials(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// Sort the material ranges by name first, and index second
			std::sort(
				pCtx->m_mtlRanges.begin(),
				pCtx->m_mtlRanges.end(),
				[](const MtlRange & a, const MtlRange & b)
				{
					if (a.m_mtlName != b.m_mtlName)
						return a.m_mtlName < b.m_mtlName;
					else
						return a.m_iIdxStart < b.m_iIdxStart;
				});

			// Reorder the indices to make them contiguous given the new
			// order of the material ranges, and merge together all ranges
			// that use the same material.

			std::vector<MtlRange> mtlRangesMerged;
			mtlRangesMerged.reserve(pCtx->m_mtlRanges.size());

			std::vector<int> indicesReordered;
			indicesReordered.resize(pCtx->m_indices.size());

			int indicesCopied = 0;

			// Copy the first range
			{
				const MtlRange & rangeFirst = pCtx->m_mtlRanges[0];
				memcpy(
					&indicesReordered[0],
					&pCtx->m_indices[rangeFirst.m_iIdxStart],
					rangeFirst.m_cIdx * sizeof(int));

				MtlRange rangeMerged = { rangeFirst.m_mtlName, 0, rangeFirst.m_cIdx, };
				mtlRangesMerged.push_back(rangeMerged);

				indicesCopied = rangeFirst.m_cIdx;
			}

			// Copy and merge the rest of the ranges
			for (int i = 1, cRange = int(pCtx->m_mtlRanges.size()); i < cRange; ++i)
			{
				const MtlRange & rangeCur = pCtx->m_mtlRanges[i];
				memcpy(
					&indicesReordered[indicesCopied],
					&pCtx->m_indices[rangeCur.m_iIdxStart],
					rangeCur.m_cIdx * sizeof(int));
				
				if (rangeCur.m_mtlName == mtlRangesMerged.back().m_mtlName)
				{
					// Material name is the same as the last range, so just extend it
					mtlRangesMerged.back().m_cIdx += rangeCur.m_cIdx;
				}
				else
				{
					// Different material name, so create a new range
					MtlRange rangeMerged = { rangeCur.m_mtlName, indicesCopied, rangeCur.m_cIdx, };
					mtlRangesMerged.push_back(rangeMerged);
				}

				indicesCopied += rangeCur.m_cIdx;
			}

			ASSERT_ERR(indicesCopied == pCtx->m_indices.size());
			ASSERT_ERR(mtlRangesMerged.size() <= pCtx->m_mtlRanges.size());

			pCtx->m_indices.swap(indicesReordered);
			pCtx->m_mtlRanges.swap(mtlRangesMerged);
		}

		static void SerializeMaterialMap(Context * pCtx, std::vector<byte> * pDataOut)
		{
			ASSERT_ERR(pCtx);
			ASSERT_ERR(pDataOut);

			// Just write out each string in null-terminated format, followed by the index range
			for (int i = 0, cRange = int(pCtx->m_mtlRanges.size()); i < cRange; ++i)
			{
				const MtlRange & range = pCtx->m_mtlRanges[i];

				int iByteStart = int(pDataOut->size());

				// Write the material name
				int nameLength = int(range.m_mtlName.size()) + 1;
				pDataOut->resize(iByteStart + nameLength + 2 * sizeof(int));
				memcpy(&(*pDataOut)[iByteStart], range.m_mtlName.c_str(), nameLength);

				// Write the index range
				memcpy(&(*pDataOut)[iByteStart + nameLength], &range.m_iIdxStart, 2 * sizeof(int));
			}
		}
	}
}
