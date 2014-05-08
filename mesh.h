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
		std::vector<Vertex>			m_verts;
		std::vector<uint>			m_indices;

		comptr<ID3D11Buffer>		m_pVtxBuffer;
		comptr<ID3D11Buffer>		m_pIdxBuffer;
		uint						m_vtxStride;	// Vertex stride for IASetVertexBuffers
		uint						m_cIdx;			// Index count for DrawIndexed
		D3D11_PRIMITIVE_TOPOLOGY	m_primtopo;
		box3						m_box;			// Bounding box in local space

		Mesh();
		void Draw(ID3D11DeviceContext * pCtx);
		void Release();
	};

	bool LoadObjMesh(
		ID3D11Device * pDevice,
		const char * path,
		Mesh * pMeshOut);
}
