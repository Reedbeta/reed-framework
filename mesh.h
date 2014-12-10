#pragma once

// Vertex tangents are disabled for now...but they can be turned back on here
#define VERTEX_TANGENT 0

namespace Framework
{
	// Hard-coded vertex struct for now
	struct Vertex
	{
		point3	m_pos;
		float3	m_normal;
		float2	m_uv;
#if VERTEX_TANGENT
		float3	m_tangent;
#endif
	};

	class Mesh
	{
	public:
		// Asset pack that this mesh's data is sourced from
		comptr<AssetPack>			m_pPack;

		// Pointers to vertex and index data in the asset pack
		Vertex *					m_pVerts;
		int *						m_pIndices;
		int							m_vertCount;
		int							m_indexCount;

		// !!!UNDONE: material map

		// GPU resources
		comptr<ID3D11Buffer>		m_pVtxBuffer;
		comptr<ID3D11Buffer>		m_pIdxBuffer;

		// Rendering info
		int							m_vtxStrideBytes;
		D3D11_PRIMITIVE_TOPOLOGY	m_primtopo;
		box3						m_bounds;			// Bounding box in local space

				Mesh();
		void	Draw(ID3D11DeviceContext * pCtx);
		void	Reset();

		// Creates the vertex and index buffers on the GPU from m_pVerts and m_pIndices
		void	UploadToGPU(ID3D11Device * pDevice);
	};

	bool LoadMeshFromAssetPack(
		AssetPack * pPack,
		const char * path,
		Mesh * pMeshOut);
}
