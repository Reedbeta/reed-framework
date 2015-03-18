#include "framework.h"
#include "asset-internal.h"
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

		using namespace AssetCompiler;
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
		static bool ParseOBJ(const char * path, Context * pCtxOut)
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
			TextParsingHelper tph((char *)&data[0]);
			while (tph.NextLine())
			{
				char * pToken = tph.NextToken();
				if (_stricmp(pToken, "v") == 0)
				{
					char * tokens[3] = {};
					tph.ExpectTokens(tokens, dim(tokens), path, "vertex position");

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
					tph.ExpectTokens(tokens, dim(tokens), path, "normal vector");

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
					tph.ExpectTokens(tokens, dim(tokens), path, "UVs");

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
					const char * pMtlName = tph.ExpectOneToken(path, "material name");
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

		static void SerializeMaterialMap(Context * pCtx, std::vector<byte> * pDataOut)
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

	static bool DeserializeMaterialMap(const byte * pMtlMap, int mtlMapSize, MaterialLib * pMtlLib, Mesh * pMeshOut);

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

	static bool DeserializeMaterialMap(const byte * pMtlMap, int mtlMapSize, MaterialLib * pMtlLib, Mesh * pMeshOut)
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
