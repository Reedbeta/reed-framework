#include "mesh.h"
#include <cassert>
#include <unordered_map>

namespace Framework
{
	Mesh::Mesh()
	:	m_verts(),
		m_indices(),
		m_pVtxBuffer(),
		m_pIdxBuffer(),
		m_vtxStride(0),
		m_cIdx(0),
		m_primtopo(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED),
		m_box(makebox3Empty())
	{
	}

	void Mesh::Draw(ID3D11DeviceContext * pCtx)
	{
		uint zero = 0;
		pCtx->IASetVertexBuffers(0, 1, &m_pVtxBuffer, &m_vtxStride, &zero);
		pCtx->IASetIndexBuffer(m_pIdxBuffer, DXGI_FORMAT_R32_UINT, 0);
		pCtx->IASetPrimitiveTopology(m_primtopo);
		pCtx->DrawIndexed(m_cIdx, 0, 0);
	}

	void Mesh::Release()
	{
		m_verts.clear();
		m_indices.clear();
		m_pVtxBuffer.release();
		m_pIdxBuffer.release();
	}

	// Mesh loading - helper functions

	static bool LoadObjMeshRaw(
		const char * path,
		std::vector<Vertex> * pVerts,
		std::vector<uint> * pIndices,
		box3 * pBoxOut,
		bool * pHasNormalsOut)
	{
		// Read the whole file into memory
		std::vector<byte> data;
		if (!loadFile(path, &data, true))
		{
			return false;
		}

		std::vector<point3> positions;
		std::vector<float3> normals;
		std::vector<float2> uvs;

		struct OBJVertex { uint iPos, iNormal, iUv; };
		std::vector<OBJVertex> OBJverts;

		struct OBJFace { uint iVertStart, iVertEnd; };
		std::vector<OBJFace> OBJfaces;

		// Parse the OBJ format line-by-line
		char * pCtxLine = (char *)&data[0];
		for (char * pLine = tokenize(pCtxLine, "\n");
			 pLine;
			 pLine = tokenize(pCtxLine, "\n"))
		{
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
				// Add vertex
				point3 pos;
				pos.x = float(atof(tokenize(pCtxToken, " \t")));
				pos.y = float(atof(tokenize(pCtxToken, " \t")));
				pos.z = float(atof(tokenize(pCtxToken, " \t")));
				positions.push_back(pos);
			}
			else if (_stricmp(pToken, "vn") == 0)
			{
				// Add normal
				float3 normal;
				normal.x = float(atof(tokenize(pCtxToken, " \t")));
				normal.y = float(atof(tokenize(pCtxToken, " \t")));
				normal.z = float(atof(tokenize(pCtxToken, " \t")));
				normals.push_back(normal);
			}
			else if (_stricmp(pToken, "vt") == 0)
			{
				// Add UV, flipping V-axis since OBJ is stored in the opposite convention
				float2 uv;
				uv.x = float(atof(tokenize(pCtxToken, " \t")));
				uv.y = 1.0f - float(atof(tokenize(pCtxToken, " \t")));
				uvs.push_back(uv);
			}
			else if (_stricmp(pToken, "f") == 0)
			{
				// Add face
				OBJFace face;
				face.iVertStart = uint(OBJverts.size());

				while (char * pToken = tokenize(pCtxToken, " \t"))
				{
					// Parse vertex specification, with slashes separating position, UV, normal indices
					// Note that some components may be missing and will be set to zero here
					OBJVertex vert = {};

					char * pCtxVert = pToken;
					if (char * pIdx = tokenize(pCtxVert, "/"))
						vert.iPos = atoi(pIdx);
					if (char * pIdx = tokenize(pCtxVert, "/"))
						vert.iUv = atoi(pIdx);
					vert.iNormal = atoi(pCtxVert);
				
					OBJverts.push_back(vert);
				}

				face.iVertEnd = uint(OBJverts.size());
				OBJfaces.push_back(face);
			}
			else
			{
				// Unknown command; just ignore
			}
		}

		// Convert to vertex buffer and index buffer

		pVerts->reserve(OBJverts.size());

		for (uint iVert = 0, cVert = uint(OBJverts.size()); iVert < cVert; ++iVert)
		{
			OBJVertex objv = OBJverts[iVert];
			Vertex v = {};

			// OBJ indices are 1-based; handle that and look out for missing components
			if (objv.iPos > 0)
				v.m_pos = positions[objv.iPos - 1];
			if (objv.iNormal > 0)
				v.m_normal = normals[objv.iNormal - 1];
			if (objv.iUv > 0)
				v.m_uv = uvs[objv.iUv - 1];

			pVerts->push_back(v);
		}

		for (uint iFace = 0, cFace = uint(OBJfaces.size()); iFace < cFace; ++iFace)
		{
			OBJFace face = OBJfaces[iFace];

			int iVertBase = face.iVertStart;

			// Triangulate the face
			for (uint iVert = face.iVertStart + 2; iVert < face.iVertEnd; ++iVert)
			{
				pIndices->push_back(iVertBase);
				pIndices->push_back(iVert - 1);
				pIndices->push_back(iVert);
			}
		}

		if (pBoxOut)
			*pBoxOut = makebox3(uint(positions.size()), &positions[0]);

		if (pHasNormalsOut)
			*pHasNormalsOut = !normals.empty();

		return true;
	}

	static void DeduplicateVerts(Mesh * pMesh)
	{
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

		struct VertexEquator
		{
			bool operator () (const Vertex & u, const Vertex & v) const
			{
				return (all(u.m_pos == v.m_pos) &&
						all(u.m_normal == v.m_normal) &&
						all(u.m_uv == v.m_uv));
			}
		};

		std::vector<Vertex> vertsDeduplicated;
		std::vector<uint> remappingTable;
		std::unordered_map<Vertex, int, VertexHasher, VertexEquator> mapVertToIndex;

		vertsDeduplicated.reserve(pMesh->m_verts.size());
		remappingTable.reserve(pMesh->m_verts.size());

		for (uint i = 0, cVert = uint(pMesh->m_verts.size()); i < cVert; ++i)
		{
			const Vertex & vert = pMesh->m_verts[i];
			std::unordered_map<Vertex, int, VertexHasher, VertexEquator>::iterator
				iter = mapVertToIndex.find(vert);
			if (iter == mapVertToIndex.end())
			{
				// Found a new vertex that's not in the map yet.
				uint newIndex = uint(vertsDeduplicated.size());
				vertsDeduplicated.push_back(vert);
				remappingTable.push_back(newIndex);
				mapVertToIndex[vert] = newIndex;
			}
			else
			{
				// It's already in the map; re-use the previous index
				uint newIndex = iter->second;
				remappingTable.push_back(newIndex);
			}
		}

		assert(vertsDeduplicated.size() <= pMesh->m_verts.size());
		assert(remappingTable.size() == pMesh->m_verts.size());

		std::vector<uint> indicesRemapped;
		indicesRemapped.reserve(pMesh->m_indices.size());

		for (uint i = 0, cIndex = uint(pMesh->m_indices.size()); i < cIndex; ++i)
		{
			indicesRemapped.push_back(remappingTable[pMesh->m_indices[i]]);
		}

		pMesh->m_verts.swap(vertsDeduplicated);
		pMesh->m_indices.swap(indicesRemapped);
	}

	static void CalculateNormals(Mesh * pMesh)
	{
		assert(pMesh->m_indices.size() % 3 == 0);

		// Generate a normal for each triangle, and accumulate onto vertex
		for (uint i = 0, c = uint(pMesh->m_indices.size()); i < c; i += 3)
		{
			uint indices[3] = { pMesh->m_indices[i], pMesh->m_indices[i+1], pMesh->m_indices[i+2] };

			// Gather positions for this triangle
			point3 facePositions[3] =
			{
				pMesh->m_verts[indices[0]].m_pos,
				pMesh->m_verts[indices[1]].m_pos,
				pMesh->m_verts[indices[2]].m_pos,
			};

			// Calculate edge and normal vectors
			float3 edge0 = facePositions[1] - facePositions[0];
			float3 edge1 = facePositions[2] - facePositions[0];
			float3 normal = normalize(cross(edge0, edge1));

			// Accumulate onto vertices
			pMesh->m_verts[indices[0]].m_normal += normal;
			pMesh->m_verts[indices[1]].m_normal += normal;
			pMesh->m_verts[indices[2]].m_normal += normal;
		}

		// Normalize summed normals
		for (uint i = 0, c = uint(pMesh->m_verts.size()); i < c; ++i)
		{
			pMesh->m_verts[i].m_normal = normalize(pMesh->m_verts[i].m_normal);
		}
	}

#if VERTEX_TANGENT
	static void CalculateTangents(Mesh * pMesh)
	{
		assert(pMesh->m_indices.size() % 3 == 0);

		// Generate a tangent for each triangle, based on triangle's UV mapping,
		// and accumulate onto vertex
		for (uint i = 0, c = uint(pMesh->m_indices.size()); i < c; i += 3)
		{
			uint indices[3] = { pMesh->m_indices[i], pMesh->m_indices[i+1], pMesh->m_indices[i+2] };

			// Gather positions for this triangle
			point3 facePositions[3] =
			{
				pMesh->m_verts[indices[0]].m_pos,
				pMesh->m_verts[indices[1]].m_pos,
				pMesh->m_verts[indices[2]].m_pos,
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
				pMesh->m_verts[indices[0]].m_uv,
				pMesh->m_verts[indices[1]].m_uv,
				pMesh->m_verts[indices[2]].m_uv,
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
			pMesh->m_verts[indices[0]].m_tangent += tangent;
			pMesh->m_verts[indices[1]].m_tangent += tangent;
			pMesh->m_verts[indices[2]].m_tangent += tangent;
		}

		// Normalize summed tangents
		for (uint i = 0, c = uint(pMesh->m_verts.size()); i < c; ++i)
		{
			pMesh->m_verts[i].m_tangent = normalize(pMesh->m_verts[i].m_tangent);
		}
	}
#endif // VERTEX_TANGENT

	bool LoadObjMesh(
		const char * path,
		ID3D11Device * pDevice,
		Mesh * pMeshOut)
	{
		bool hasNormals;
		if (!LoadObjMeshRaw(
				path,
				&pMeshOut->m_verts,
				&pMeshOut->m_indices,
				&pMeshOut->m_box,
				&hasNormals))
		{
			return false;
		}

		DeduplicateVerts(pMeshOut);

		LOG("Loaded %s - %d verts, %d indices",
			path, pMeshOut->m_verts.size(), pMeshOut->m_indices.size());

		// !!!UNDONE: vertex cache optimization?

		if (!hasNormals)
			CalculateNormals(pMeshOut);

#if VERTEX_TANGENT
		CalculateTangents(pMeshOut);
#endif

		// Upload the mesh data to the GPU

		D3D11_BUFFER_DESC vtxBufferDesc =
		{
			sizeof(Vertex) * uint(pMeshOut->m_verts.size()),
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_VERTEX_BUFFER,
			0,	// no cpu access
			0,	// no misc flags
			0,	// structured buffer stride
		};
		D3D11_SUBRESOURCE_DATA vtxBufferData = { &pMeshOut->m_verts[0], 0, 0 };

		if (FAILED(pDevice->CreateBuffer(&vtxBufferDesc, &vtxBufferData, &pMeshOut->m_pVtxBuffer)))
		{
			assert(false);
			return false;
		}

		D3D11_BUFFER_DESC idxBufferDesc =
		{
			sizeof(uint) * uint(pMeshOut->m_indices.size()),
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_INDEX_BUFFER,
			0,	// no cpu access
			0,	// no misc flags
			0,	// structured buffer stride
		};
		D3D11_SUBRESOURCE_DATA idxBufferData = { &pMeshOut->m_indices[0], 0, 0 };

		if (FAILED(pDevice->CreateBuffer(&idxBufferDesc, &idxBufferData, &pMeshOut->m_pIdxBuffer)))
		{
			assert(false);
			return false;
		}

		pMeshOut->m_vtxStride = sizeof(Vertex);
		pMeshOut->m_cIdx = uint(pMeshOut->m_indices.size());
		pMeshOut->m_primtopo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		return true;
	}
}
