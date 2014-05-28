#pragma once

namespace Framework
{
	class RenderTarget
	{
	public:
				RenderTarget();

		void	Init(
					ID3D11Device * pDevice,
					uint width, uint height,
					DXGI_FORMAT format);

		void	Bind(ID3D11DeviceContext * pCtx);

		comptr<ID3D11Texture2D>				m_pTex;
		comptr<ID3D11RenderTargetView>		m_pRtv;
		comptr<ID3D11ShaderResourceView>	m_pSrv;
		uint								m_width, m_height;
	};
}
