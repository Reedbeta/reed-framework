#pragma once

// Vertex tangents are disabled for now...but they can be turned back on here
#define VERTEX_TANGENT 0

namespace Framework
{
	struct Material;
	class MaterialLib;

	// Hard-coded vertex struct for now
	struct Vertex
	{
		float3	m_pos;
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

		// Material map
		struct MtlRange
		{
			Material *	m_pMtl;
			int			m_indexStart, m_indexCount;
		};
		std::vector<MtlRange>		m_mtlRanges;

		// GPU resources
		comptr<ID3D11Buffer>		m_pVtxBuffer;
		comptr<ID3D11Buffer>		m_pIdxBuffer;

		// Rendering info
		int							m_vtxStrideBytes;
		D3D11_PRIMITIVE_TOPOLOGY	m_primtopo;
		box3						m_bounds;			// Bounding box in local space

				Mesh();
		void	Draw(ID3D11DeviceContext * pCtx);
		void	DrawMtlRange(ID3D11DeviceContext * pCtx, int iMtlRange);
		void	Reset();

		// Creates the vertex and index buffers on the GPU from m_pVerts and m_pIndices
		void	UploadToGPU(ID3D11Device * pDevice);
	};

	// Load a mesh from an asset pack and resolve material references
	// using the given texture library
	bool LoadMeshFromAssetPack(
		AssetPack * pPack,
		const char * path,
		MaterialLib * pMtlLib,
		Mesh * pMeshOut);

	// Helper function for quick and dirty apps - just get a mesh from an
	// .obj file, no messing around with asset packs or materials
	bool LoadOBJMesh(
		const char * path,
		Mesh * pMeshOut);
}
