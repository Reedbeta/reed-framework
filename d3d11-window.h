#pragma once

namespace Framework
{
	class D3D11Window
	{
	public:
							D3D11Window();
		virtual				~D3D11Window() {}

		bool				Init(
								const char * windowClassName,
								const char * windowTitle,
								HINSTANCE hInstance);
		virtual void		Shutdown();
		int					MainLoop(int nShowCmd);

		virtual LRESULT		MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		virtual void		OnResize(int width, int height);
		virtual void		OnRender() = 0;

		// Basic resources
		HINSTANCE							m_hInstance;
		HWND								m_hWnd;
		comptr<IDXGISwapChain>				m_pSwapChain;
		comptr<ID3D11Device>				m_pDevice;
		comptr<ID3D11DeviceContext>			m_pCtx;
		int									m_width, m_height;

		// Back buffer render target views, one as SRGB and one as raw
		comptr<ID3D11RenderTargetView>		m_pRtvSRGB;
		comptr<ID3D11RenderTargetView>		m_pRtvRaw;

		// Screen depth buffer
		comptr<ID3D11DepthStencilView>		m_pDsv;
		comptr<ID3D11ShaderResourceView>	m_pSrvDepth;

		// Commonly used state blocks
		comptr<ID3D11RasterizerState>		m_pRsDefault;
		comptr<ID3D11RasterizerState>		m_pRsDoubleSided;
		comptr<ID3D11DepthStencilState>		m_pDssDepthTest;
		comptr<ID3D11DepthStencilState>		m_pDssNoDepthWrite;
		comptr<ID3D11DepthStencilState>		m_pDssNoDepthTest;
		comptr<ID3D11BlendState>			m_pBsAlphaBlend;

		// Commonly used samplers
		comptr<ID3D11SamplerState>			m_pSsPointClamp;
		comptr<ID3D11SamplerState>			m_pSsBilinearClamp;
		comptr<ID3D11SamplerState>			m_pSsTrilinearRepeat;
		comptr<ID3D11SamplerState>			m_pSsTrilinearRepeatAniso;
		comptr<ID3D11SamplerState>			m_pSsPCF;
	};
}
