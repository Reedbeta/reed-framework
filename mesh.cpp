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
}
