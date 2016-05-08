#pragma once

namespace Framework
{
	struct CBBlit		// matches cbuffer CBBlit in fullscreen_vs.hlsl, rect_vs.hlsl
	{
		box2	m_boxSrc;
		box2	m_boxDst;
	};

	struct LineVertex	// Matches struct LineVertex in lines_vs.hlsl and lines_ps.hlsl
	{
		float4	m_rgba;
		float4	m_posClip;
	};

	class D3D11Window
	{
	public:
							D3D11Window();
		virtual				~D3D11Window() {}

		void				Init(
								const char * windowClassName,
								const char * windowTitle,
								HINSTANCE hInstance);
		virtual void		Shutdown();
		void				MainLoop(int nShowCmd);

		virtual LRESULT		MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		virtual void		OnResize(int2 dimsNew);
		virtual void		OnRender() = 0;

		// Utility methods

		void				BindSRGBBackBuffer(ID3D11DeviceContext * pCtx);
		void				BindRawBackBuffer(ID3D11DeviceContext * pCtx);

		void				SetViewport(ID3D11DeviceContext * pCtx, int2 dims);
		void				SetViewport(ID3D11DeviceContext * pCtx, box2 viewport);
		void				SetViewport(ID3D11DeviceContext * pCtx, box3 viewport);

		void				DrawFullscreenPass(
								ID3D11DeviceContext * pCtx,
								box2 boxSrc = { 0, 0, 1, 1 });
		void				DrawRectPass(
								ID3D11DeviceContext * pCtx,
								box2 boxDst)
								{ DrawRectPass(pCtx, box2{ 0, 0, 1, 1 }, boxDst); }
		void				DrawRectPass(
								ID3D11DeviceContext * pCtx,
								box2 boxSrc,
								box2 boxDst);

		void				BlitFullscreen(
								ID3D11DeviceContext * pCtx,
								ID3D11ShaderResourceView * pSrvSrc,
								box2 boxSrc = { 0, 0, 1, 1 })
								{ BlitFullscreen(pCtx, pSrvSrc, m_pSsBilinearClamp, boxSrc); }
		void				BlitFullscreen(
								ID3D11DeviceContext * pCtx,
								ID3D11ShaderResourceView * pSrvSrc,
								ID3D11SamplerState * pSampSrc,
								box2 boxSrc = { 0, 0, 1, 1 });

		void				Blit(
								ID3D11DeviceContext * pCtx,
								ID3D11ShaderResourceView * pSrvSrc,
								box2 boxDst)
								{ Blit(pCtx, pSrvSrc, m_pSsBilinearClamp, box2{ 0, 0, 1, 1 }, boxDst); }
		void				Blit(
								ID3D11DeviceContext * pCtx,
								ID3D11ShaderResourceView * pSrvSrc,
								ID3D11SamplerState * pSampSrc,
								box2 boxSrc,
								box2 boxDst);

		// Methods for debug lines
		void				AddDebugLine(float2 p0, float2 p1, rgba rgba);
		void				AddDebugLine(float2 p0, float2 p1, rgba rgba, float3x3 const & xfm);
		void				AddDebugLine(float4 p0, float4 p1, rgba rgba);
		void				AddDebugLine(float4 p0, float4 p1, rgba rgba, float4x4 const & xfm);
		void				AddDebugLineStrip(const float2 * pPoints, int numPoints, rgba rgba);
		void				AddDebugLineStrip(const float2 * pPoints, int numPoints, rgba rgba, float3x3 const & xfm);
		void				AddDebugLineStrip(const float4 * pPoints, int numPoints, rgba rgba);
		void				AddDebugLineStrip(const float4 * pPoints, int numPoints, rgba rgba, float4x4 const & xfm);
		void				DrawDebugLines(ID3D11DeviceContext * pCtx);

		// Basic resources
		HINSTANCE							m_hInstance;
		HWND								m_hWnd;
		comptr<IDXGISwapChain>				m_pSwapChain;
		comptr<ID3D11Device>				m_pDevice;
		comptr<ID3D11DeviceContext>			m_pCtx;
		int2								m_dims;

		// Back buffer render target views, one as SRGB and one as raw
		comptr<ID3D11Texture2D>				m_pTexBackBuffer;
		comptr<ID3D11RenderTargetView>		m_pRtvSRGB;
		comptr<ID3D11RenderTargetView>		m_pRtvRaw;

		// Screen depth buffer
		bool								m_hasDepthBuffer;
		comptr<ID3D11Texture2D>				m_pTexDepth;
		comptr<ID3D11DepthStencilView>		m_pDsv;
		comptr<ID3D11ShaderResourceView>	m_pSrvDepth;

		// Commonly used state blocks
		comptr<ID3D11RasterizerState>		m_pRsDefault;
		comptr<ID3D11RasterizerState>		m_pRsDoubleSided;
		comptr<ID3D11DepthStencilState>		m_pDssDepthTest;
		comptr<ID3D11DepthStencilState>		m_pDssNoDepthWrite;
		comptr<ID3D11DepthStencilState>		m_pDssNoDepthTest;
		comptr<ID3D11BlendState>			m_pBsAdditive;
		comptr<ID3D11BlendState>			m_pBsAlphaBlend;

		// Commonly used samplers
		comptr<ID3D11SamplerState>			m_pSsPointClamp;
		comptr<ID3D11SamplerState>			m_pSsBilinearClamp;
		comptr<ID3D11SamplerState>			m_pSsTrilinearRepeat;
		comptr<ID3D11SamplerState>			m_pSsTrilinearRepeatAniso;
		comptr<ID3D11SamplerState>			m_pSsPCF;

		// Commonly used shaders
		comptr<ID3D11VertexShader>			m_pVsFullscreen;
		comptr<ID3D11VertexShader>			m_pVsRect;
		comptr<ID3D11PixelShader>			m_pPsCopy;

		// CB for doing blits and fullscreen passes
		CB<CBBlit>							m_cbBlit;

		// Stuff for drawing debug lines
		std::vector<LineVertex>				m_lineVertices;
		comptr<ID3D11Buffer>				m_pBufLineVertices;
		comptr<ID3D11InputLayout>			m_pInputLayoutLines;
		comptr<ID3D11VertexShader>			m_pVsLines;
		comptr<ID3D11PixelShader>			m_pPsLines;
	};
}
