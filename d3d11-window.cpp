#include "d3d11-window.h"

#include <util.h>
#include <cassert>

namespace Framework
{
	static LRESULT CALLBACK StaticMsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	D3D11Window::D3D11Window()
	:	m_hInstance(nullptr),
		m_hWnd(nullptr),
		m_pSwapChain(nullptr),
		m_pDevice(nullptr),
		m_pCtx(nullptr),
		m_width(0),
		m_height(0)
	{
	}

	bool D3D11Window::Init(
		const char * windowClassName,
		const char * windowTitle,
		HINSTANCE hInstance,
		int nShowCmd)
	{
		m_hInstance = hInstance;

		// Register window class
		WNDCLASS wc =
		{
			0,
			&StaticMsgProc,
			0,
			0,
			hInstance,
			LoadIcon(NULL, IDI_APPLICATION),
			LoadCursor(NULL, IDC_ARROW),
			nullptr,
			nullptr,
			windowClassName,
		};
		if (!RegisterClass(&wc))
		{
			assert(false);
			return false;
		}

		// Create the window
		m_hWnd = CreateWindow(
					windowClassName,
					windowTitle,
					WS_OVERLAPPEDWINDOW,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					NULL,
					NULL,
					hInstance,
					this);
		if (!m_hWnd)
		{
			assert(false);
			return false;
		}

		// Initialize D3D11
		UINT flags = 0;
#ifdef _DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		DXGI_SWAP_CHAIN_DESC swapChainDesc =
		{
			{ 0, 0, {}, DXGI_FORMAT_R8G8B8A8_UNORM, },
			{ 1, 0, },
			DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER,
			2,
			m_hWnd,
			true,
			DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
		};
		D3D_FEATURE_LEVEL featureLevel;
		if (FAILED(D3D11CreateDeviceAndSwapChain(
						nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
						flags, nullptr, 0, D3D11_SDK_VERSION,
						&swapChainDesc,
						&m_pSwapChain, &m_pDevice,
						&featureLevel, &m_pCtx)))
		{
			assert(false);
			return false;
		}

		// Show the window
		ShowWindow(m_hWnd, nShowCmd);

		return true;
	}

	void D3D11Window::Shutdown()
	{
		m_hInstance = nullptr;
		m_width = 0;
		m_height = 0;

		SafeRelease(m_pCtx);
		SafeRelease(m_pDevice);
		SafeRelease(m_pSwapChain);

		if (m_hWnd)
		{
			DestroyWindow(m_hWnd);
			m_hWnd = nullptr;
		}
	}

	int D3D11Window::MainLoop()
	{
		MSG msg;

		for (;;)
		{
			// Handle any messages
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				DispatchMessage(&msg);
			}

			// Quit if necessary
			if (msg.message == WM_QUIT)
				break;

			// Render the frame
			OnRender();
		}

		// Return code specified in PostQuitMessage() ends up in wParam of WM_QUIT
		return int(msg.wParam);
	}

	static LRESULT CALLBACK StaticMsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		D3D11Window * pWindow;

		if (message == WM_CREATE)
		{
			// On creation, stash pointer to the D3D11Window object in the window data
			CREATESTRUCT * pCreate = (CREATESTRUCT *)lParam;
			pWindow = (D3D11Window *)pCreate->lpCreateParams;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, LONG_PTR(pWindow));
		}
		else
		{
			// Retrieve the D3D11Window object from the window data
			pWindow = (D3D11Window *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			// If it's not there yet (due to messages prior to WM_CREATE),
			// just fall back to DefWindowProc
			if (!pWindow)
				return DefWindowProc(hWnd, message, wParam, lParam);
		}

		// Pass the message to the D3D11Window object
		return pWindow->MsgProc(hWnd, message, wParam, lParam);
	}

	LRESULT D3D11Window::MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CLOSE:
			Shutdown();
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_SIZE:
			{
				int width = int(LOWORD(lParam));
				int height = int(HIWORD(lParam));
				if (width > 0 && height > 0 && (width != m_width || height != m_height))
					OnResize(width, height);
				return 0;
			}

		case WM_SIZING:
			{
				RECT clientRect;
				GetClientRect(hWnd, &clientRect);
				int width = clientRect.right - clientRect.left;
				int height = clientRect.bottom - clientRect.top;
				if (width > 0 && height > 0 && (width != m_width || height != m_height))
					OnResize(width, height);
				return 0;
			}

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}

	void D3D11Window::OnResize(int width, int height)
	{
		m_width = width;
		m_height = height;
		LOG("OnResize(%d, %d)\n", width, height);
	}
}
