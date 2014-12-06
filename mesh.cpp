#include "framework.h"

namespace Framework
{
	Mesh::Mesh()
	:	m_pVerts(nullptr),
		m_pIndices(nullptr),
		m_vertCount(0),
		m_indexCount(0),
		m_vtxStrideBytes(0),
		m_primtopo(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED),
		m_bounds(makebox3Empty())
	{
	}

	void Mesh::Draw(ID3D11DeviceContext * pCtx)
	{
		UINT zero = 0;
		pCtx->IASetVertexBuffers(0, 1, &m_pVtxBuffer, (UINT *)&m_vtxStrideBytes, &zero);
		pCtx->IASetIndexBuffer(m_pIdxBuffer, DXGI_FORMAT_R32_UINT, 0);
		pCtx->IASetPrimitiveTopology(m_primtopo);
		pCtx->DrawIndexed(int(m_indexCount), 0, 0);
	}

	void Mesh::Reset()
	{
		m_pPack.release();
		m_pVerts = nullptr;
		m_pIndices = nullptr;
		m_vertCount = 0;
		m_indexCount = 0;
		m_pVtxBuffer.release();
		m_pIdxBuffer.release();
		m_vtxStrideBytes = 0;
		m_primtopo = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		m_bounds = makebox3Empty();
	}

	void Mesh::UploadToGPU(ID3D11Device * pDevice)
	{
		ASSERT_ERR(pDevice);

		m_pVtxBuffer.release();
		m_pIdxBuffer.release();

		D3D11_BUFFER_DESC vtxBufferDesc =
		{
			sizeof(Vertex) * m_vertCount,
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_VERTEX_BUFFER,
			0,	// no cpu access
			0,	// no misc flags
			0,	// structured buffer stride
		};
		D3D11_SUBRESOURCE_DATA vtxBufferData = { m_pVerts, 0, 0 };
		CHECK_D3D(pDevice->CreateBuffer(&vtxBufferDesc, &vtxBufferData, &m_pVtxBuffer));

		D3D11_BUFFER_DESC idxBufferDesc =
		{
			sizeof(int) * m_indexCount,
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_INDEX_BUFFER,
			0,	// no cpu access
			0,	// no misc flags
			0,	// structured buffer stride
		};
		D3D11_SUBRESOURCE_DATA idxBufferData = { m_pIndices, 0, 0 };
		CHECK_D3D(pDevice->CreateBuffer(&idxBufferDesc, &idxBufferData, &m_pIdxBuffer));

		m_vtxStrideBytes = sizeof(Vertex);
		m_primtopo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}

	bool LoadMeshFromAssetPack(
		AssetPack * pPack,
		const char * path,
		Mesh * pMeshOut)
	{
		ASSERT_ERR(pPack);
		ASSERT_ERR(path);
		ASSERT_ERR(pMeshOut);

		extern const char * g_suffixVerts;
		extern const char * g_suffixIndices;
		extern const char * g_suffixMtlMap;
		extern const char * g_suffixBounds;

		// Look for the data in the asset pack

		int vertsSize;
		if (!pPack->LookupFile(path, g_suffixVerts, (void **)&pMeshOut->m_pVerts, &vertsSize))
		{
			WARN("Couldn't find verts for mesh %s in asset pack %s", path, pPack->m_path.c_str());
			return false;
		}
		pMeshOut->m_vertCount = vertsSize / sizeof(Vertex);

		int indicesSize;
		if (!pPack->LookupFile(path, g_suffixIndices, (void **)&pMeshOut->m_pIndices, &indicesSize))
		{
			WARN("Couldn't find indices for mesh %s in asset pack %s", path, pPack->m_path.c_str());
			return false;
		}
		pMeshOut->m_indexCount = indicesSize / sizeof(int);

		// !!!UNDONE: material map

		box3 * pBounds;
		int boundsSize;
		if (!pPack->LookupFile(path, g_suffixBounds, (void **)&pBounds, &boundsSize))
		{
			WARN("Couldn't find bounds for mesh %s in asset pack %s", path, pPack->m_path.c_str());
			return false;
		}
		ASSERT_ERR(boundsSize == sizeof(box3));
		pMeshOut->m_bounds = *pBounds;

		LOG("Loaded %s from asset pack %s - %d verts, %d indices",
			path, pPack->m_path.c_str(), pMeshOut->m_vertCount, pMeshOut->m_indexCount);

		return true;
	}

#if OLD
	bool LoadObjMesh(
		ID3D11Device * pDevice,
		const char * path,
		Mesh * pMeshOut)
	{
		ASSERT_ERR(pDevice);
		ASSERT_ERR(path);
		ASSERT_ERR(pMeshOut);

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

		pMeshOut->DeduplicateVerts();

		// !!!UNDONE: vertex cache optimization?

		if (!hasNormals)
			pMeshOut->CalculateNormals();

#if VERTEX_TANGENT
		pMeshOut->CalculateTangents();
#endif

		pMeshOut->UploadToGPU(pDevice);

		LOG("Loaded %s - %d verts, %d indices",
			path, pMeshOut->m_verts.size(), pMeshOut->m_indices.size());

		return true;
	}
#endif
}
