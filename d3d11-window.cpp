#include "d3d11-window.h"
#include <cstdio>

namespace Framework
{
	static LRESULT CALLBACK StaticMsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	D3D11Window::D3D11Window()
	:	m_hInstance(nullptr),
		m_hWnd(nullptr),
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
		int nShowCmd,
		DXGI_FORMAT backBufferFormat /*= DXGI_FORMAT_R8G8B8A8_UNORM_SRGB*/,
		int backBufferSampleCount /*= 4*/)
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
			return false;

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
			return false;

		ShowWindow(m_hWnd, nShowCmd);

		(void)backBufferFormat;
		(void)backBufferSampleCount;

		return true;
	}

	void D3D11Window::Shutdown()
	{
		m_hInstance = nullptr;
		m_width = 0;
		m_height = 0;

		SafeRelease(m_pDevice);
		SafeRelease(m_pCtx);

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
		char msg[128];
		sprintf_s(msg, "OnResize(%d, %d)\n", width, height);
		OutputDebugString(msg);
	}
}
