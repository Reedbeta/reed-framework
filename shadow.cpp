#include "framework.h"

namespace Framework
{
	// ShadowMap implementation

	ShadowMap::ShadowMap()
	:	m_vecLight(0.0f),
		m_boundsScene(empty),
		m_matProj(0.0f),
		m_matWorldToClip(0.0f),
		m_matWorldToUvzw(0.0f),
		m_matWorldToUvzNormal(0.0f),
		m_vecDiam(0.0f)
	{
	}

	void ShadowMap::Init(
		ID3D11Device * pDevice,
		int2 dims,
		DXGI_FORMAT format /* = DXGI_FORMAT_D32_FLOAT */)
	{
		m_dst.Init(pDevice, dims, format);
	
		LOG("Created shadow map - %dx%d, %s", dims.x, dims.y, NameOfFormat(format));
	}

	void ShadowMap::Reset()
	{
		m_dst.Reset();
		m_vecLight = float3(0.0f);
		m_boundsScene = box3(empty);
		m_matProj = float4x4(0.0f);
		m_matWorldToClip = float4x4(0.0f);
		m_matWorldToUvzw = float4x4(0.0f);
		m_matWorldToUvzNormal = float3x3(0.0f);
		m_vecDiam = float3(0.0f);
	}

	void ShadowMap::UpdateMatrix()
	{
		// Calculate view matrix based on light direction

		// Choose a world-space up-vector
		float3 vecUp = { 0.0f, 0.0f, 1.0f };
		if (all(isnear(m_vecLight.xy, 0.0f)))
			vecUp = { 1.0f, 0.0f, 0.0f };

		affine3 viewToWorld = affineMatrix(lookatZMatrix3D(-m_vecLight, vecUp), float3(0.0f));
		affine3 worldToView = inverseRigid(viewToWorld);

		// Transform scene AABB into view space and recalculate bounds
		box3 boundsView = xfmBox(m_boundsScene, worldToView);
		float3 vecDiamOriginal = boundsView.maxs - boundsView.mins;

		// Select maximum diameter along X and Y, so that shadow map texels will be square
		float maxXY = max(vecDiamOriginal.x, vecDiamOriginal.y);
		m_vecDiam = { maxXY, maxXY, vecDiamOriginal.z };
		boundsView = boxExpandAllSides(boundsView, 0.5f * (m_vecDiam - vecDiamOriginal));

		// Calculate orthogonal projection matrix to fit the scene bounds
		m_matProj = orthoProjD3DStyle(
						boundsView.mins.x,
						boundsView.maxs.x,
						boundsView.mins.y,
						boundsView.maxs.y,
						-boundsView.maxs.z,
						-boundsView.mins.z);

		m_matWorldToClip = worldToView * m_matProj;

		// Calculate alternate matrix that maps to [0, 1] UV space instead of [-1, 1] clip space
		float4x4 matClipToUvzw =
		{
			0.5f,  0,    0, 0,
			0,    -0.5f, 0, 0,
			0,     0,    1, 0,
			0.5f,  0.5f, 0, 1,
		};
		m_matWorldToUvzw = m_matWorldToClip * matClipToUvzw;

		// Calculate inverse transpose matrix for transforming normals
		m_matWorldToUvzNormal = transpose(inverse(float3x3(m_matWorldToUvzw)));
	}

	void ShadowMap::Bind(ID3D11DeviceContext * pCtx)
	{
		m_dst.Bind(pCtx);
	}
}
