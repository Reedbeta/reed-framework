#include "framework.h"

namespace Framework
{
	RenderTarget::RenderTarget()
	:	m_pTex(),
		m_pRtv(),
		m_pSrv(),
		m_dims(makeuint2(0))
	{
	}

	void RenderTarget::Init(
		ID3D11Device * pDevice,
		uint2_arg dims,
		DXGI_FORMAT format)
	{
		D3D11_TEXTURE2D_DESC texDesc =
		{
			dims.x, dims.y, 1, 1,
			format,
			{ 1, 0 },
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
			0, 0,
		};
		CHECK_D3D(pDevice->CreateTexture2D(&texDesc, nullptr, &m_pTex));

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc =
		{
			format,
			D3D11_RTV_DIMENSION_TEXTURE2D,
		};
		CHECK_D3D(pDevice->CreateRenderTargetView(m_pTex, &rtvDesc, &m_pRtv));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc =
		{
			format,
			D3D11_SRV_DIMENSION_TEXTURE2D,
		};
		srvDesc.Texture2D.MipLevels = 1;
		CHECK_D3D(pDevice->CreateShaderResourceView(m_pTex, &srvDesc, &m_pSrv));

		m_dims = dims;
	}

	void RenderTarget::Bind(ID3D11DeviceContext * pCtx)
	{
		pCtx->OMSetRenderTargets(1, &m_pRtv, nullptr);
		D3D11_VIEWPORT viewport = { 0.0f, 0.0f, float(m_dims.x), float(m_dims.y), 0.0f, 1.0f, };
		pCtx->RSSetViewports(1, &viewport);
	}
}
