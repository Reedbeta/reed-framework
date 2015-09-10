#include "framework.h"
#include "asset-internal.h"
#include <algorithm>
#include <deque>

namespace Framework
{
	// Infrastructure for compiling Wavefront .obj files to vertex/index buffers.
	//  * Currently uses hard-coded Vertex structure.
	//  * Creates a single vertex buffer and index buffer, plus a material map that
	//      identifies which faces get drawn with each material.
	//  * Groups together all faces with the same material into a contiguous
	//      range of indices, so they can be drawn with one draw call.
	//  * Removes degenerate triangles.
	//  * Deduplicates verts.
	//  * Generates normals if necessary.
	//  * !!!UNDONE: Vertex cache optimization.

	namespace OBJMeshCompiler
	{
		static const char * s_suffixMeta		= "/meta";
		static const char * s_suffixVerts		= "/verts";
		static const char * s_suffixIndices		= "/indices";
		static const char * s_suffixMtlMap		= "/material_map";

		struct MtlRange
		{
			std::string		m_mtlName;
			int				m_indexStart, m_indexCount;
		};

		struct Context
		{
			std::vector<Vertex>		m_verts;
			std::vector<int>		m_indices;
			std::vector<MtlRange>	m_mtlRanges;
			box3					m_bounds;
			bool					m_hasNormals;
		};

		struct Meta
		{
			// Later: vertex format info

			box3			m_bounds;
		};

		// Prototype various helper functions
		bool ParseOBJ(const char * path, Context * pCtxOut);
		void RemoveDegenerateTriangles(Context * pCtx);
		void RemoveEmptyMaterialRanges(Context * pCtx);
		void DeduplicateVerts(Context * pCtx);
		void CalculateNormals(Context * pCtx);
		void NormalizeNormals(Context * pCtx);
#if VERTEX_TANGENT
		void CalculateTangents(Context * pCtx);
#endif
		void SortMaterials(Context * pCtx);
		void SortTrianglesForVertexCache(Context * pCtx);
		void SortVerticesForMemoryCache(Context * pCtx);
		float ComputeACMR(const Context * pCtx, int cacheSize = 32);

		void SerializeMaterialMap(Context * pCtx, std::vector<byte> * pDataOut);
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

		using namespace AssetCompiler;
		using namespace OBJMeshCompiler;

		// Read the mesh data from the OBJ file
		Context ctx = {};
		if (!ParseOBJ(pACI->m_pathSrc, &ctx))
			return false;

		// Clean up the mesh
		SortMaterials(&ctx);
		RemoveDegenerateTriangles(&ctx);
		RemoveEmptyMaterialRanges(&ctx);
		DeduplicateVerts(&ctx);
		if (!ctx.m_hasNormals)
			CalculateNormals(&ctx);
		NormalizeNormals(&ctx);
#if VERTEX_TANGENT
		CalculateTangents(&ctx);
#endif
		SortTrianglesForVertexCache(&ctx);
		SortVerticesForMemoryCache(&ctx);

#if 0
		// This can take awhile on a big mesh, so it's commented out by default
		LOG("%s ACMR: %0.2f", pACI->m_pathSrc, ComputeACMR(&ctx));
#endif

		// Fill out the metadata struct
		Meta meta =
		{
			ctx.m_bounds,
		};

		// Write the data out to the archive

		std::vector<byte> serializedMaterialMap;
		SerializeMaterialMap(&ctx, &serializedMaterialMap);

		if (!WriteAssetDataToZip(pACI->m_pathSrc, s_suffixMeta, &meta, sizeof(meta), pZipOut) ||
			!WriteAssetDataToZip(pACI->m_pathSrc, s_suffixVerts, &ctx.m_verts[0], ctx.m_verts.size() * sizeof(Vertex), pZipOut) ||
			!WriteAssetDataToZip(pACI->m_pathSrc, s_suffixIndices, &ctx.m_indices[0], ctx.m_indices.size() * sizeof(int), pZipOut) ||
			!WriteAssetDataToZip(pACI->m_pathSrc, s_suffixMtlMap, &serializedMaterialMap[0], serializedMaterialMap.size(), pZipOut))
		{
			return false;
		}

		return true;
	}



	namespace OBJMeshCompiler
	{
		bool ParseOBJ(const char * path, Context * pCtxOut)
		{
			ASSERT_ERR(path);
			ASSERT_ERR(pCtxOut);

			// Read the whole file into memory
			std::vector<byte> data;
			if (!LoadFile(path, &data, LFK_Text))
				return false;

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

			// Parse line-by-line
			TextParsingHelper tph((char *)&data[0], path);
			while (tph.NextLine())
			{
				char * pToken = tph.NextToken();
				if (_stricmp(pToken, "v") == 0)
				{
					char * tokens[3] = {};
					tph.ExpectTokens(tokens, dim(tokens), "vertex position");
					tph.ExpectEOL();

					// Add vertex
					point3 pos;
					pos.x = float(atof(tokens[0]));
					pos.y = float(atof(tokens[1]));
					pos.z = float(atof(tokens[2]));
					positions.push_back(pos);
				}
				else if (_stricmp(pToken, "vn") == 0)
				{
					char * tokens[3] = {};
					tph.ExpectTokens(tokens, dim(tokens), "normal vector");
					tph.ExpectEOL();

					// Add normal
					float3 normal;
					normal.x = float(atof(tokens[0]));
					normal.y = float(atof(tokens[1]));
					normal.z = float(atof(tokens[2]));
					normals.push_back(normal);
				}
				else if (_stricmp(pToken, "vt") == 0)
				{
					char * tokens[2] = {};
					tph.ExpectTokens(tokens, dim(tokens), "UVs");

					// OBJ files can have a third texture coordinate, but
					// currently we just throw it away if it's there
					(void)tph.NextToken();
					tph.ExpectEOL();

					// Add UV, flipping V-axis since OBJ UVs use a bottom-up convention
					float2 uv;
					uv.x = float(atof(tokens[0]));
					uv.y = 1.0f - float(atof(tokens[1]));
					uvs.push_back(uv);
				}
				else if (_stricmp(pToken, "f") == 0)
				{
					// Add face
					OBJFace face = {};
					face.iVertStart = int(OBJverts.size());

					while (char * pCtxVert = tph.NextToken())
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

						// Handle negative indices - a bizarre OBJ feature that lets you reference
						// verts by counting backward from the most recent one
						if (vert.iPos < 0)
							vert.iPos += int(positions.size()) + 1;
						if (vert.iUv < 0)
							vert.iUv += int(uvs.size()) + 1;
						if (vert.iNormal < 0)
							vert.iNormal += int(normals.size()) + 1;

						OBJverts.push_back(vert);
					}

					face.iVertEnd = int(OBJverts.size());

					if (face.iVertEnd == face.iVertStart)
					{
						WARN("%s: syntax error at line %d: missing faces", path, tph.m_iLine);
						continue;
					}

					OBJfaces.push_back(face);
				}
				else if (_stricmp(pToken, "usemtl") == 0)
				{
					const char * pMtlName = tph.ExpectOneToken("material name");
					tph.ExpectEOL();
					if (!pMtlName)
						continue;

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
					makeLowercase(pRange->mtlName);
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

		void RemoveDegenerateTriangles(Context * pCtx)
		{
			ASSERT_ERR(pCtx);
			ASSERT_ERR(pCtx->m_indices.size() % 3 == 0);

			// Remove degenerate triangles by compacting in-place
			int iWrite = 0;
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

				// Triangle is degenerate if normal is near-zero
				bool degenerate = all(isnear(normal, 0.0f));
				if (degenerate)
				{
					// Fix up material ranges.  This could be done more efficiently, but on the
					// assumption that degenerate triangles are rare and material ranges are few,
					// this should be OK...
					for (int iMaterial = 0, cMaterial = int(pCtx->m_mtlRanges.size()); iMaterial < cMaterial; ++iMaterial)
					{
						MtlRange * pMtlRange = &pCtx->m_mtlRanges[iMaterial];
						if (iWrite < pMtlRange->m_indexStart)
							pMtlRange->m_indexStart -= 3;
						else if (iWrite < pMtlRange->m_indexStart + pMtlRange->m_indexCount)
							pMtlRange->m_indexCount -= 3;
					}
				}
				else
				{
					// Not degenerate: keep this triangle; copy its indices down to the write cursor
					pCtx->m_indices[iWrite  ] = pCtx->m_indices[i  ];
					pCtx->m_indices[iWrite+1] = pCtx->m_indices[i+1];
					pCtx->m_indices[iWrite+2] = pCtx->m_indices[i+2];
					iWrite += 3;
				}
			}

			ASSERT_ERR(iWrite <= int(pCtx->m_indices.size()));
			pCtx->m_indices.resize(iWrite);
		}

		void RemoveEmptyMaterialRanges(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// Remove empty material ranges by compacting in-place
			int iWrite = 0;
			for (int i = 0, c = int(pCtx->m_mtlRanges.size()); i < c; ++i)
			{
				MtlRange * pMtlRange = &pCtx->m_mtlRanges[i];

				ASSERT_ERR(pMtlRange->m_indexCount >= 0);

				// Skip empty ones
				if (pMtlRange->m_indexCount == 0)
					continue;

				if (iWrite < i)
				{
					// Not empty: keep this material; move it down to the write cursor
					pCtx->m_mtlRanges[iWrite] = std::move(*pMtlRange);
				}

				++iWrite;
			}

			ASSERT_ERR(iWrite <= int(pCtx->m_mtlRanges.size()));
			pCtx->m_mtlRanges.resize(iWrite);
		}

		void DeduplicateVerts(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// Set up a hash table for vertices

			struct VertexHasher
			{
				std::hash<float> fh;
				size_t operator () (const Vertex & v) const
				{
					return fh(v.m_pos.x) ^ fh(v.m_pos.y) ^ fh(v.m_pos.z) ^
						   fh(v.m_normal.x) ^ fh(v.m_normal.y) ^ fh(v.m_normal.z) ^
						   fh(v.m_uv.x) ^ fh(v.m_uv.y);
					// Note: m_tangent not included because it isn't part of the .obj format,
					// and hasn't been computed yet at this stage in the compilation process
				}
			};

			struct VertexEqualityTester
			{
				bool operator () (const Vertex & u, const Vertex & v) const
				{
					return (all(u.m_pos == v.m_pos) &&
							all(u.m_normal == v.m_normal) &&
							all(u.m_uv == v.m_uv));
					// Note: m_tangent not included because it isn't part of the .obj format,
					// and hasn't been computed yet at this stage in the compilation process
				}
			};

			std::vector<Vertex> vertsDeduplicated;
			std::vector<int> remappingTable;
			std::unordered_map<Vertex, int, VertexHasher, VertexEqualityTester> mapVertToIndex;
			std::vector<int> indicesRemapped;

			vertsDeduplicated.reserve(pCtx->m_verts.size());
			remappingTable.resize(pCtx->m_verts.size(), -1);
			mapVertToIndex.reserve(pCtx->m_verts.size());
			indicesRemapped.resize(pCtx->m_indices.size());

			// Iterate over indices, so that we automatically skip orphaned vertices
			for (int i = 0, c = int(pCtx->m_indices.size()); i < c; ++i)
			{
				int index = pCtx->m_indices[i];

				// If this vertex was already remapped, update the index
				if (remappingTable[index] >= 0)
				{
					indicesRemapped[i] = remappingTable[index];
					continue;
				}

				// Search the hash table for a match for this vertex
				const Vertex & vert = pCtx->m_verts[index];
				auto iter = mapVertToIndex.find(vert);
				if (iter == mapVertToIndex.end())
				{
					// Found a new vertex that's not in the table yet
					int newIndex = int(vertsDeduplicated.size());
					vertsDeduplicated.push_back(vert);
					remappingTable[index] = newIndex;
					mapVertToIndex.insert(std::make_pair(vert, newIndex));
					indicesRemapped[i] = newIndex;
				}
				else
				{
					// It's already in the map; re-use the previous index
					int newIndex = iter->second;
					remappingTable[index] = newIndex;
					indicesRemapped[i] = newIndex;
				}
			}

			ASSERT_ERR(vertsDeduplicated.size() <= pCtx->m_verts.size());

			pCtx->m_verts.swap(vertsDeduplicated);
			pCtx->m_indices.swap(indicesRemapped);
		}

		void CalculateNormals(Context * pCtx)
		{
			ASSERT_ERR(pCtx);
			ASSERT_ERR(pCtx->m_indices.size() % 3 == 0);

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
				ASSERT_WARN(all(isfinite(normal)));

				// Accumulate onto vertices
				pCtx->m_verts[indices[0]].m_normal += normal;
				pCtx->m_verts[indices[1]].m_normal += normal;
				pCtx->m_verts[indices[2]].m_normal += normal;
			}
		}

		void NormalizeNormals(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// Normalize summed normals
			for (int i = 0, c = int(pCtx->m_verts.size()); i < c; ++i)
			{
				pCtx->m_verts[i].m_normal = normalize(pCtx->m_verts[i].m_normal);
				ASSERT_WARN(all(isfinite(pCtx->m_verts[i].m_normal)));
			}
		}

#if VERTEX_TANGENT
		void CalculateTangents(Context * pCtx)
		{
			ASSERT_ERR(pCtx);
			ASSERT_ERR(pCtx->m_indices.size() % 3 == 0);

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

		void SortMaterials(Context * pCtx)
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
						return a.m_indexStart < b.m_indexStart;
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
					&pCtx->m_indices[rangeFirst.m_indexStart],
					rangeFirst.m_indexCount * sizeof(int));

				MtlRange rangeMerged = { rangeFirst.m_mtlName, 0, rangeFirst.m_indexCount, };
				mtlRangesMerged.push_back(rangeMerged);

				indicesCopied = rangeFirst.m_indexCount;
			}

			// Copy and merge the rest of the ranges
			for (int i = 1, cRange = int(pCtx->m_mtlRanges.size()); i < cRange; ++i)
			{
				const MtlRange & rangeCur = pCtx->m_mtlRanges[i];
				memcpy(
					&indicesReordered[indicesCopied],
					&pCtx->m_indices[rangeCur.m_indexStart],
					rangeCur.m_indexCount * sizeof(int));
				
				if (rangeCur.m_mtlName == mtlRangesMerged.back().m_mtlName)
				{
					// Material name is the same as the last range, so just extend it
					mtlRangesMerged.back().m_indexCount += rangeCur.m_indexCount;
				}
				else
				{
					// Different material name, so create a new range
					MtlRange rangeMerged = { rangeCur.m_mtlName, indicesCopied, rangeCur.m_indexCount, };
					mtlRangesMerged.push_back(rangeMerged);
				}

				indicesCopied += rangeCur.m_indexCount;
			}

			ASSERT_ERR(indicesCopied == pCtx->m_indices.size());
			ASSERT_ERR(mtlRangesMerged.size() <= pCtx->m_mtlRanges.size());

			pCtx->m_indices.swap(indicesReordered);
			pCtx->m_mtlRanges.swap(mtlRangesMerged);
		}

		void SortTrianglesForVertexCache(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			// Implementation of "Linear-Speed Vertex Cache Optimization" by Tom Forsyth
			// https://home.comcast.net/~tom_forsyth/papers/fast_vert_cache_opt.html

			static const int s_cacheSize = 32;

			// Initialize ancillary data that we keep per vertex and per triangle

			struct ExtraVertexData
			{
				int	cachePosition;
				float score;
				int triangles;			// Count of not-yet-sorted triangles using this vertex
				int iTriStart;			// Index into trianglesByVert where this vertex's triangles start

				void RecalcScore()
				{
					// Verts with no unsorted triangles remaining are no longer in play
					if (triangles == 0)
					{
						score = -1.0f;
						return;
					}

					// Calculate part of score based on cache position
					float cacheScore;
					if (cachePosition < 0)
					{
						cacheScore = 0.0f;	// Not in the cache: score zero
					}
					else if (cachePosition < 3)
					{
						// Fixed score for verts in the latest triangle - disfavors them slightly
						// relative to other recently-used verts, to avoid excessive strip-ifying
						cacheScore = 0.75f;
					}
					else
					{
						// Calculate score based on how recently it was used
						ASSERT_ERR(cachePosition < s_cacheSize);
						static const float scale = 1.0f / (s_cacheSize - 3);
						cacheScore = powf(1.0f - (cachePosition - 3) * scale, 1.5f);
					}

					// Calculate part of score based on number of triangles that use this vert -
					// favors verts with only a few triangles left, to favor completing a region of
					// the mesh before jumping to a new region
					float valenceScore = 2.0f * powf(float(triangles), -0.5f);

					score = cacheScore + valenceScore;
				}
			};
			std::vector<ExtraVertexData> extraVertexDatas(pCtx->m_verts.size());

			struct ExtraTriData
			{
				float score;	// Sum of vertex scores, or set to -1 when triangle is sorted
			};
			std::vector<ExtraTriData> extraTriDatas;

			std::vector<int> trianglesByVert;

			// Sort each material range's triangles separately
			for (int iRange = 0, cRange = int(pCtx->m_mtlRanges.size()); iRange < cRange; ++iRange)
			{
				MtlRange range = pCtx->m_mtlRanges[iRange];

				ASSERT_ERR(range.m_indexCount > 0 && range.m_indexCount % 3 == 0);

				memset(&extraVertexDatas[0], 0, sizeof(ExtraVertexData) * extraVertexDatas.size());
				extraTriDatas.resize(range.m_indexCount / 3);
				memset(&extraTriDatas[0], 0, sizeof(ExtraTriData) * extraTriDatas.size());
				trianglesByVert.assign(range.m_indexCount, -1);	// Each triangle is in exactly 3 verts' lists

				// Build table of references from verts to triangles that use them

				// Count triangles per vertex
				for (int iIdx = range.m_indexStart, iIdxEnd = range.m_indexStart + range.m_indexCount;
					 iIdx < iIdxEnd; ++iIdx)
				{
					++extraVertexDatas[pCtx->m_indices[iIdx]].triangles;
				}

				// Build list of triangles per vertex, also calculate initial scores for verts
				// and triangles, and keep track of the best triangle found
				int trianglesByVertAllocated = 0;
				int bestTri = -1;
				float bestTriScore = 0.0f;
				for (int iIdx = range.m_indexStart, iIdxEnd = range.m_indexStart + range.m_indexCount;
					 iIdx < iIdxEnd; ++iIdx)
				{
					ExtraVertexData * pEvd = &extraVertexDatas[pCtx->m_indices[iIdx]];
					
					// We reuse cachePosition as the negative index where to store the next triangle
					// index into the vert's list of triangles.  Negative values indicate the vertex
					// is not in cache, so this also sets up for the following section.
					int iTriStore;
					if (pEvd->cachePosition < 0)
					{
						// Find where to add the current triangle to the list
						iTriStore = pEvd->iTriStart + (-pEvd->cachePosition);
						--pEvd->cachePosition;
					}
					else
					{
						// Allocate space for triangle indices
						pEvd->iTriStart = trianglesByVertAllocated;
						iTriStore = trianglesByVertAllocated;
						trianglesByVertAllocated += pEvd->triangles;
						pEvd->cachePosition = -1;

						// Also calculate initial vertex score
						pEvd->RecalcScore();
					}

					// Store the triangle to the array
					ASSERT_ERR(iTriStore < int(trianglesByVert.size()));
					ASSERT_ERR(iTriStore - pEvd->iTriStart < pEvd->triangles);
					ASSERT_ERR(trianglesByVert[iTriStore] == -1);
					int iTri = (iIdx - range.m_indexStart) / 3;
					trianglesByVert[iTriStore] = iTri;

					// Add the vertex score into the triangle score
					ExtraTriData * pEtd = &extraTriDatas[iTri];
					pEtd->score += pEvd->score;

					// Keep track of the best triangle seen
					if (pEtd->score > bestTriScore)
					{
						bestTri = iTri;
						bestTriScore = pEtd->score;
					}
				}

				ASSERT_ERR(trianglesByVertAllocated == int(trianglesByVert.size()));
				ASSERT_ERR(bestTri >= 0 && bestTri < int(extraTriDatas.size()));

				int vertexCache[2][s_cacheSize + 3] = {};
				for (int i = 0; i < dim(vertexCache); ++i)
					for (int j = 0; j < dim(vertexCache[0]); ++j)
						vertexCache[i][j] = -1;

				std::vector<int> indicesReordered;
				indicesReordered.reserve(range.m_indexCount);
				
				// Iterate through triangles, picking the one to add to indicesReordered next
				for (int iTriAdd = 0, cTriAdd = range.m_indexCount/3;;)
				{
					// Add the best triangle seen so far to the new indices
					int indicesAdd[3] =
					{
						pCtx->m_indices[range.m_indexStart + 3*bestTri],
						pCtx->m_indices[range.m_indexStart + 3*bestTri + 1],
						pCtx->m_indices[range.m_indexStart + 3*bestTri + 2],
					};
					indicesReordered.insert(indicesReordered.end(), &indicesAdd[0], &indicesAdd[dim(indicesAdd)]);

					++iTriAdd;
					if (iTriAdd >= cTriAdd)
						break;

					// Reset the triangle's score to indicate that it's been sorted
					extraTriDatas[bestTri].score = -1.0f;

					// Update the vertices
					for (int i = 0; i < dim(indicesAdd); ++i)
					{
						ExtraVertexData * pEvd = &extraVertexDatas[indicesAdd[i]];

						// Remove the triangle we just added from the vertex's list of triangles
						auto it = std::find(
									&trianglesByVert[pEvd->iTriStart], 
									&trianglesByVert[pEvd->iTriStart] + pEvd->triangles,
									bestTri);
						ASSERT_ERR(it < &trianglesByVert[pEvd->iTriStart] + pEvd->triangles);
						*it = trianglesByVert[pEvd->iTriStart + pEvd->triangles - 1];

						// Decrement the not-yet-sorted-triangles count
						--pEvd->triangles;
					}

					// Update the LRU cache, putting the newly used vertices at the top
					// (and preserving the order of the other elements)
					int * vertexCachePrev = vertexCache[iTriAdd & 1];
					int * vertexCacheNext = vertexCache[!(iTriAdd & 1)];
					vertexCacheNext[0] = indicesAdd[0];
					vertexCacheNext[1] = indicesAdd[1];
					vertexCacheNext[2] = indicesAdd[2];
					int iCacheWrite = 3;
					for (int iRead = 0; iRead < s_cacheSize; ++iRead)
					{
						int cachedVal = vertexCachePrev[iRead];
						if (cachedVal < 0)
							break;
						if (cachedVal != indicesAdd[0] &&
							cachedVal != indicesAdd[1] &&
							cachedVal != indicesAdd[2])
						{
							vertexCacheNext[iCacheWrite] = cachedVal;
							++iCacheWrite;
						}
					}
					ASSERT_ERR(iCacheWrite <= dim(vertexCache[0]));

					// Update the cache indices of all the verts and recompute their scores
					for (int i = 0; i < iCacheWrite; ++i)
					{
						ExtraVertexData * pEvd = &extraVertexDatas[vertexCacheNext[i]];
						pEvd->cachePosition = (i >= s_cacheSize) ? -1 : i;
						pEvd->RecalcScore();
					}

					// Recompute the scores of tris that use verts in the cache,
					// and keep track of the new best tri as we go
					bestTri = -1;
					bestTriScore = 0.0f;
					for (int i = 0; i < iCacheWrite; ++i)
					{
						ExtraVertexData * pEvd = &extraVertexDatas[vertexCacheNext[i]];

						// Update all the unsorted tris that use this vertex
						for (int j = 0; j < pEvd->triangles; ++j)
						{
							int iTri = trianglesByVert[pEvd->iTriStart + j];
							int indices[3] =
							{
								pCtx->m_indices[range.m_indexStart + 3*iTri],
								pCtx->m_indices[range.m_indexStart + 3*iTri + 1],
								pCtx->m_indices[range.m_indexStart + 3*iTri + 2],
							};
							float triScore = extraVertexDatas[indices[0]].score +
											 extraVertexDatas[indices[1]].score +
											 extraVertexDatas[indices[2]].score;
							extraTriDatas[iTri].score = triScore;

							if (triScore > bestTriScore)
							{
								bestTri = iTri;
								bestTriScore = triScore;
							}
						}
					}

					// If we didn't find a tri above (e.g. because all verts in the cache are
					// out of unsorted tris) then fallback to searching the entire list of tris
					if (bestTri < 0)
					{
						for (int i = 0, cTri = int(extraTriDatas.size()); i < cTri; ++i)
						{
							float triScore = extraTriDatas[i].score;
							if (triScore > bestTriScore)
							{
								bestTri = i;
								bestTriScore = triScore;
							}
						}
					}

					ASSERT_ERR(bestTri >= 0 && bestTri < int(extraTriDatas.size()));
				}

				ASSERT_ERR(int(indicesReordered.size()) == range.m_indexCount);

				// Replace the old indices with the new indices for this range
				memcpy(&pCtx->m_indices[range.m_indexStart], &indicesReordered[0], sizeof(int) * range.m_indexCount);
			}
		}

		void SortVerticesForMemoryCache(Context * pCtx)
		{
			ASSERT_ERR(pCtx);

			std::vector<Vertex> vertsReordered;
			std::vector<int> remappingTable;
			std::vector<int> indicesRemapped;

			vertsReordered.reserve(pCtx->m_verts.size());
			remappingTable.resize(pCtx->m_verts.size(), -1);
			indicesRemapped.resize(pCtx->m_indices.size());

			// Iterate over indices, so that we see vertices in the order they'll be fetched
			for (int i = 0, c = int(pCtx->m_indices.size()); i < c; ++i)
			{
				int index = pCtx->m_indices[i];

				// If this vertex was already remapped, update the index
				if (remappingTable[index] >= 0)
				{
					indicesRemapped[i] = remappingTable[index];
					continue;
				}

				// Append vertex to the reordered buffer and record its new index
				int newIndex = int(vertsReordered.size());
				vertsReordered.push_back(pCtx->m_verts[index]);
				remappingTable[index] = newIndex;
				indicesRemapped[i] = newIndex;
			}

			ASSERT_ERR(vertsReordered.size() == pCtx->m_verts.size());

			pCtx->m_verts.swap(vertsReordered);
			pCtx->m_indices.swap(indicesRemapped);
		}

		float ComputeACMR(const Context * pCtx, int cacheSize /*= 32*/)
		{
			// Compute the average cache miss rate (ACMR) of the mesh.  This is the number of
			// vertices per triangle that miss the cache.  Worst case is 3.0, and for typical
			// connected meshes, values between 0.6 and 0.8 are considered very good.

			// Vertex cache is a FIFO cache rather than LRU (simulates hardware better).
			std::deque<int> vertexCache;

			int indexCount = int(pCtx->m_indices.size());
			int missCount = 0;
			for (int i = 0; i < indexCount; ++i)
			{
				int index = pCtx->m_indices[i];

				// If we don't find it in the cache, it's a miss
				auto it = std::find(vertexCache.begin(), vertexCache.end(), index);
				if (it == vertexCache.end())
				{
					// Add it to the back of the FIFO cache
					++missCount;
					vertexCache.push_back(index);
					if (int(vertexCache.size()) > cacheSize)
						vertexCache.pop_front();
				}
			}

			ASSERT_ERR(int(vertexCache.size()) <= cacheSize);

			return float(missCount) / float(max(indexCount / 3, 1));
		}

		void SerializeMaterialMap(Context * pCtx, std::vector<byte> * pDataOut)
		{
			ASSERT_ERR(pCtx);
			ASSERT_ERR(pDataOut);

			SerializeHelper sh(pDataOut);
			for (int i = 0, cRange = int(pCtx->m_mtlRanges.size()); i < cRange; ++i)
			{
				const MtlRange & range = pCtx->m_mtlRanges[i];
				sh.WriteString(range.m_mtlName);
				sh.Write(range.m_indexStart);
				sh.Write(range.m_indexCount);
			}
		}
	}



	// Load compiled data into a runtime game object

	bool DeserializeMaterialMap(const byte * pMtlMap, int mtlMapSize, MaterialLib * pMtlLib, Mesh * pMeshOut);

	bool LoadMeshFromAssetPack(
		AssetPack * pPack,
		const char * path,
		MaterialLib * pMtlLib,
		Mesh * pMeshOut)
	{
		ASSERT_ERR(pPack);
		ASSERT_ERR(path);
		ASSERT_ERR(pMeshOut);

		using namespace OBJMeshCompiler;

		pMeshOut->m_pPack = pPack;

		// Look for the data in the asset pack

		Meta * pMeta;
		int metaSize;
		if (!pPack->LookupFile(path, s_suffixMeta, (void **)&pMeta, &metaSize))
		{
			WARN("Couldn't find metadata for mesh %s in asset pack %s", path, pPack->m_path.c_str());
			return false;
		}
		if (metaSize != sizeof(Meta))
		{
			WARN("Metadata for mesh %s in asset pack %s is wrong size, %d bytes (expected %d)",
				path, pPack->m_path.c_str(), metaSize, sizeof(Meta));
			return false;
		}
		pMeshOut->m_bounds = pMeta->m_bounds;

		int vertsSize;
		if (!pPack->LookupFile(path, s_suffixVerts, (void **)&pMeshOut->m_pVerts, &vertsSize))
		{
			WARN("Couldn't find verts for mesh %s in asset pack %s", path, pPack->m_path.c_str());
			return false;
		}
		pMeshOut->m_vertCount = vertsSize / sizeof(Vertex);

		int indicesSize;
		if (!pPack->LookupFile(path, s_suffixIndices, (void **)&pMeshOut->m_pIndices, &indicesSize))
		{
			WARN("Couldn't find indices for mesh %s in asset pack %s", path, pPack->m_path.c_str());
			return false;
		}
		pMeshOut->m_indexCount = indicesSize / sizeof(int);

		byte * pMtlMap;
		int mtlMapSize;
		if (!pPack->LookupFile(path, s_suffixMtlMap, (void **)&pMtlMap, &mtlMapSize))
		{
			WARN("Couldn't find material map for mesh %s in asset pack %s", path, pPack->m_path.c_str());
			return false;
		}
		if (!DeserializeMaterialMap(pMtlMap, mtlMapSize, pMtlLib, pMeshOut))
		{
			WARN("Couldn't deserialize material map for mesh %s in asset pack %s", path, pPack->m_path.c_str());
			return false;
		}

		LOG("Loaded %s from asset pack %s - %d verts, %d indices, %d materials",
			path, pPack->m_path.c_str(), pMeshOut->m_vertCount, pMeshOut->m_indexCount, pMeshOut->m_mtlRanges.size());

		return true;
	}

	bool DeserializeMaterialMap(const byte * pMtlMap, int mtlMapSize, MaterialLib * pMtlLib, Mesh * pMeshOut)
	{
		ASSERT_ERR(pMtlMap);
		ASSERT_ERR(mtlMapSize > 0);
		ASSERT_ERR(pMeshOut);

		DeserializeHelper dh(pMtlMap, mtlMapSize);
		while (!dh.AtEOF())
		{
			Mesh::MtlRange range = {};

			// Read data
			const char * mtlName;
			if (!dh.ReadString(&mtlName) ||
				!dh.Read(&range.m_indexStart) ||
				!dh.Read(&range.m_indexCount))
			{
				return false;
			}

			// Validate data
			if (range.m_indexStart < 0 ||
				range.m_indexCount <= 0 ||
				range.m_indexStart + range.m_indexCount > pMeshOut->m_indexCount)
			{
				WARN("Corrupt material map: invalid index start/count");
				return false;
			}

			// Look up material by name
			if (pMtlLib && *mtlName)
			{
				range.m_pMtl = pMtlLib->Lookup(mtlName);
				ASSERT_WARN_MSG(range.m_pMtl, 
					"Couldn't find material %s in material library", mtlName);
			}

			pMeshOut->m_mtlRanges.push_back(range);
		}

		return true;
	}



	// Helper function for quick and dirty apps - just compile and load a mesh in one step.
	// Totally unnecessarily serializing, compressing, decompressing, and deserializing
	// all the data here...but whatever.

	bool LoadOBJMesh(
		const char * path,
		Mesh * pMeshOut)
	{
		ASSERT_ERR(path);
		ASSERT_ERR(pMeshOut);

		// Set up an in-memory zip stream
		mz_zip_archive zipWrite = {};
		CHECK_ERR(mz_zip_writer_init_heap(&zipWrite, 0, 0));

		// Compile the mesh to it
		AssetCompileInfo aci = { path, ACK_OBJMesh };
		if (!AssetCompiler::CompileFullAssetPackToZip(&aci, 1, &zipWrite))
		{
			mz_zip_writer_end(&zipWrite);
			return false;
		}

		void * pData;
		size_t sizeBytes;
		if (!mz_zip_writer_finalize_heap_archive(&zipWrite, &pData, &sizeBytes))
		{
			WARN("Couldn't finalize archive");
			mz_zip_writer_end(&zipWrite);
			return false;
		}

		// Turn around and load the pack right back in

		mz_zip_archive zipRead = {};
		CHECK_ERR(mz_zip_reader_init_mem(&zipRead, pData, sizeBytes, 0));

		AssetPack * pPack = new AssetPack;
		pPack->m_path = "(in memory)";

		if (!AssetCompiler::LoadAssetPackFromZip(&zipRead, pPack))
		{
			mz_zip_writer_end(&zipRead);
			mz_zip_writer_end(&zipWrite);
			return false;
		}

		mz_zip_writer_end(&zipRead);
		mz_zip_writer_end(&zipWrite);

		// And extract the mesh from it
		return LoadMeshFromAssetPack(pPack, path, nullptr, pMeshOut);
	}
}
