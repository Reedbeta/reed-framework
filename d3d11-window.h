#pragma once

#include "comptr.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>

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
								HINSTANCE hInstance,
								int nShowCmd);
		virtual void		Shutdown();
		int					MainLoop();

		virtual LRESULT		MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		virtual void		OnResize(int width, int height);
		virtual void		OnRender() = 0;

		HINSTANCE						m_hInstance;
		HWND							m_hWnd;
		comptr<IDXGISwapChain>			m_pSwapChain;
		comptr<ID3D11Device>			m_pDevice;
		comptr<ID3D11DeviceContext>		m_pCtx;
		comptr<ID3D11RenderTargetView>	m_pRtvSRGB;
		comptr<ID3D11RenderTargetView>	m_pRtvRaw;
		int								m_width, m_height;
	};
}
