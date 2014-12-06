#pragma once

namespace Framework
{
	int BitsPerPixel(DXGI_FORMAT format);

	enum RTFLAG
	{
		RTFLAG_EnableUAV	= 0x01,
		
		RTFLAG_Default		= 0x00,
	};

	class RenderTarget
	{
	public:
				RenderTarget();

		void	Init(
					ID3D11Device * pDevice,
					int2_arg dims,
					DXGI_FORMAT format,
					int sampleCount = 1,
					int flags = RTFLAG_Default);
		void	Reset();

		void	Bind(ID3D11DeviceContext * pCtx);
		void	Bind(ID3D11DeviceContext * pCtx, box2_arg viewport);
		void	Bind(ID3D11DeviceContext * pCtx, box3_arg viewport);

		int		SizeInBytes() const
					{ return m_dims.x * m_dims.y * m_sampleCount * BitsPerPixel(m_format) / 8; }

		// Read back the data to main memory - you're responsible for allocing enough
		void	Readback(
					ID3D11DeviceContext * pCtx,
					void * pDataOut);

		comptr<ID3D11Texture2D>				m_pTex;
		comptr<ID3D11RenderTargetView>		m_pRtv;
		comptr<ID3D11ShaderResourceView>	m_pSrv;
		comptr<ID3D11UnorderedAccessView>	m_pUav;
		int2								m_dims;
		int									m_sampleCount;
		DXGI_FORMAT							m_format;
	};

	enum DSFLAG
	{
		DSFLAG_EnableUAV	= 0x01,
		
		DSFLAG_Default		= 0x00,
	};

	class DepthStencilTarget
	{
	public:
				DepthStencilTarget();

		void	Init(
					ID3D11Device * pDevice,
					int2_arg dims,
					DXGI_FORMAT format,
					int sampleCount = 1,
					int flags = DSFLAG_Default);
		void	Reset();

		int		SizeInBytes() const
					{ return m_dims.x * m_dims.y * m_sampleCount * BitsPerPixel(m_formatDsv) / 8; }

		// Read back the data to main memory - you're responsible for allocing enough
		void	Readback(
					ID3D11DeviceContext * pCtx,
					void * pDataOut);

		comptr<ID3D11Texture2D>				m_pTex;
		comptr<ID3D11DepthStencilView>		m_pDsv;
		comptr<ID3D11ShaderResourceView>	m_pSrvDepth;
		comptr<ID3D11ShaderResourceView>	m_pSrvStencil;
		comptr<ID3D11UnorderedAccessView>	m_pUavDepth;
		comptr<ID3D11UnorderedAccessView>	m_pUavStencil;
		int2								m_dims;
		int									m_sampleCount;
		DXGI_FORMAT							m_formatDsv;
		DXGI_FORMAT							m_formatSrvDepth;
		DXGI_FORMAT							m_formatSrvStencil;
	};

	// Utility functions for binding multiple render targets
	void BindRenderTargets(ID3D11DeviceContext * pCtx, RenderTarget * pRt, DepthStencilTarget * pDst);
	void BindRenderTargets(ID3D11DeviceContext * pCtx, RenderTarget * pRt, DepthStencilTarget * pDst, box2_arg viewport);
	void BindRenderTargets(ID3D11DeviceContext * pCtx, RenderTarget * pRt, DepthStencilTarget * pDst, box3_arg viewport);
}
