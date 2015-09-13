#include "framework.h"

namespace Framework
{
	// ShadowMap implementation

	ShadowMap::ShadowMap()
	:	m_vecLight(makefloat3(0.0f)),
		m_boundsScene(makebox3Empty()),
		m_matProj(makefloat4x4(0.0f)),
		m_matWorldToClip(makefloat4x4(0.0f)),
		m_matWorldToUvzw(makefloat4x4(0.0f)),
		m_matWorldToUvzNormal(makefloat3x3(0.0f)),
		m_vecDiam(makefloat3(0.0f))
	{
	}

	void ShadowMap::Init(
		ID3D11Device * pDevice,
		int2_arg dims,
		DXGI_FORMAT format /* = DXGI_FORMAT_D32_FLOAT */)
	{
		m_dst.Init(pDevice, dims, format);
	
		LOG("Created shadow map - %dx%d, %s", dims.x, dims.y, NameOfFormat(format));
	}

	void ShadowMap::Reset()
	{
		m_dst.Reset();
		m_vecLight = makefloat3(0.0f);
		m_boundsScene = makebox3Empty();
		m_matProj = makefloat4x4(0.0f);
		m_matWorldToClip = makefloat4x4(0.0f);
		m_matWorldToUvzw = makefloat4x4(0.0f);
		m_matWorldToUvzNormal = makefloat3x3(0.0f);
		m_vecDiam = makefloat3(0.0f);
	}

	void ShadowMap::UpdateMatrix()
	{
		// Calculate view matrix based on light direction

		// Choose a world-space up-vector
		float3 vecUp = { 0.0f, 0.0f, 1.0f };
		if (all(isnear(m_vecLight.xy, 0.0f)))
			vecUp = makefloat3(1.0f, 0.0f, 0.0f);

		affine3 viewToWorld = lookatZ(-m_vecLight, vecUp);
		affine3 worldToView = transpose(viewToWorld);

		// Transform scene AABB into view space and recalculate bounds
		box3 boundsView = boxTransform(m_boundsScene, worldToView);
		float3 vecDiamOriginal = boundsView.diagonal();

		// Select maximum diameter along X and Y, so that shadow map texels will be square
		float maxXY = max(vecDiamOriginal.x, vecDiamOriginal.y);
		m_vecDiam = makefloat3(maxXY, maxXY, vecDiamOriginal.z);
		boundsView = boxGrow(boundsView, 0.5f * (m_vecDiam - vecDiamOriginal));

		// Calculate orthogonal projection matrix to fit the scene bounds
		m_matProj = orthoProjD3DStyle(
						boundsView.m_mins.x,
						boundsView.m_maxs.x,
						boundsView.m_mins.y,
						boundsView.m_maxs.y,
						-boundsView.m_maxs.z,
						-boundsView.m_mins.z);

		m_matWorldToClip = affineToHomogeneous(worldToView) * m_matProj;

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
		float3x3 matWorldToUvz = makefloat3x3(m_matWorldToUvzw);
		m_matWorldToUvzNormal = transpose(inverse(matWorldToUvz));
	}

	void ShadowMap::Bind(ID3D11DeviceContext * pCtx)
	{
		m_dst.Bind(pCtx);
	}

	float3 ShadowMap::CalcFilterUVZScale(float filterRadius)
	{
		// Expand the filter in the Z direction (this controls how far
		// the filter can tilt before it starts contracting).
		// Tuned empirically.
		float zScale = 4.0f;

		return makefloat3(
				filterRadius / m_vecDiam.x,
				filterRadius / m_vecDiam.y,
				zScale * filterRadius / m_vecDiam.z);
	}
}
