#pragma once

namespace Framework
{
	// Very simple shadow map class, fits an orthogonal shadow map around a scene bounding box
	class ShadowMap
	{
	public:
				ShadowMap();
		void	Init(
					ID3D11Device * pDevice,
					int2 dims,
					DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT);
		void	Reset();

		void	UpdateMatrix();
		void	Bind(ID3D11DeviceContext * pCtx);

		DepthStencilTarget	m_dst;
		float3				m_vecLight;					// Unit vector toward directional light
		box3				m_boundsScene;				// AABB of scene in world space

		float4x4			m_matProj;					// Projection matrix
		float4x4			m_matWorldToClip;			// Matrix for rendering shadow map
		float4x4			m_matWorldToUvzw;			// Matrix for sampling shadow map
		float3x3			m_matWorldToUvzNormal;		// Matrix for transforming normals to shadow map space
		float3				m_vecDiam;					// Diameter in world units along shadow XYZ axes
	};
}
