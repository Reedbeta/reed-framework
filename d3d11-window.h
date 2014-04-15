#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>

namespace Framework
{
	// Safely release a D3D11 object
	template <typename T>
	void SafeRelease(T * & p) { if (p) { p->Release(); p = nullptr; } }

	class D3D11Window
	{
	public:
								D3D11Window();
		virtual					~D3D11Window() {}

		bool					Init(
									const char * windowClassName,
									const char * windowTitle,
									HINSTANCE hInstance,
									int nShowCmd,
									DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
									int backBufferSampleCount = 4);
		virtual void			Shutdown();
		int						MainLoop();

		virtual LRESULT			MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		virtual void			OnResize(int width, int height);
		virtual void			OnRender() = 0;

		HINSTANCE				m_hInstance;
		HWND					m_hWnd;
		ID3D11Device *			m_pDevice;
		ID3D11DeviceContext *	m_pCtx;
		int						m_width, m_height;
	};
}
