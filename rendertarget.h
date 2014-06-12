#pragma once

namespace Framework
{
	class RenderTarget
	{
	public:
				RenderTarget();

		void	Init(
					ID3D11Device * pDevice,
					uint2_arg dims,
					DXGI_FORMAT format,
					uint sampleCount = 1);
		void	Release();

		void	Bind(ID3D11DeviceContext * pCtx);
		void	Bind(ID3D11DeviceContext * pCtx, box2_arg viewport);
		void	Bind(ID3D11DeviceContext * pCtx, box3_arg viewport);

		comptr<ID3D11Texture2D>				m_pTex;
		comptr<ID3D11RenderTargetView>		m_pRtv;
		comptr<ID3D11ShaderResourceView>	m_pSrv;
		uint2								m_dims;
		uint								m_sampleCount;
		DXGI_FORMAT							m_format;
	};
}
