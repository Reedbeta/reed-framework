#include "d3d11-window.h"

#include <util.h>
#include <cassert>

namespace Framework
{
	static LRESULT CALLBACK StaticMsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	D3D11Window::D3D11Window()
	:	m_hInstance(nullptr),
		m_hWnd(nullptr),
		m_pSwapChain(),
		m_pDevice(),
		m_pCtx(),
		m_width(0),
		m_height(0),
		m_pRtvSRGB(),
		m_pRtvRaw(),
		m_pDsv(),
		m_pSrvDepth(),
		m_pRsDefault(),
		m_pRsDoubleSided(),
		m_pDssDepthTest(),
		m_pDssNoDepthWrite(),
		m_pDssNoDepthTest(),
		m_pBsAlphaBlend(),
		m_pSsPointClamp(),
		m_pSsBilinearClamp(),
		m_pSsTrilinearRepeat(),
		m_pSsTrilinearRepeatAniso(),
		m_pSsPCF()
	{
	}

	bool D3D11Window::Init(
		const char * windowClassName,
		const char * windowTitle,
		HINSTANCE hInstance)
	{
		LOG("Initialization started");

		m_hInstance = hInstance;

		// Register window class
		WNDCLASS wc =
		{
			0,
			&StaticMsgProc,
			0,
			0,
			hInstance,
			LoadIcon(nullptr, IDI_APPLICATION),
			LoadCursor(nullptr, IDC_ARROW),
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
					nullptr,
					nullptr,
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

#if defined(_DEBUG)
		// Set up D3D11 debug layer settings
		comptr<ID3D11InfoQueue> pInfoQueue;
		if (SUCCEEDED(m_pDevice->QueryInterface(IID_ID3D11InfoQueue, (void **)&pInfoQueue)))
		{
			// Break in the debugger when an error or warning is issued
			pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
			pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
			pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);

			// Disable warning about setting private data (i.e. debug names of resources)
			D3D11_MESSAGE_ID aMsgToFilter[] =
			{
				D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
			};
			D3D11_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumIDs = dim(aMsgToFilter);
			filter.DenyList.pIDList = aMsgToFilter;
			pInfoQueue->AddStorageFilterEntries(&filter);
		}
#endif

		// Set up commonly used state blocks

		D3D11_RASTERIZER_DESC rssDesc =
		{
			D3D11_FILL_SOLID,
			D3D11_CULL_BACK,
			true,							// FrontCounterClockwise
			0, 0, 0,						// depth bias
			true,							// DepthClipEnable
			false,							// ScissorEnable
			true,							// MultisampleEnable
		};
		if (FAILED(m_pDevice->CreateRasterizerState(&rssDesc, &m_pRsDefault)))
		{
			assert(false);
			return false;
		}

		rssDesc.CullMode = D3D11_CULL_NONE;
		if (FAILED(m_pDevice->CreateRasterizerState(&rssDesc, &m_pRsDoubleSided)))
		{
			assert(false);
			return false;
		}

		D3D11_DEPTH_STENCIL_DESC dssDesc = 
		{
			true,							// DepthEnable
			D3D11_DEPTH_WRITE_MASK_ALL,
			D3D11_COMPARISON_LESS_EQUAL,
		};
		if (FAILED(m_pDevice->CreateDepthStencilState(&dssDesc, &m_pDssDepthTest)))
		{
			assert(false);
			return false;
		}

		dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		if (FAILED(m_pDevice->CreateDepthStencilState(&dssDesc, &m_pDssNoDepthWrite)))
		{
			assert(false);
			return false;
		}

		dssDesc.DepthEnable = false;
		if (FAILED(m_pDevice->CreateDepthStencilState(&dssDesc, &m_pDssNoDepthTest)))
		{
			assert(false);
			return false;
		}

		D3D11_BLEND_DESC bsDesc =
		{
			false, false,
			{
				true,
				D3D11_BLEND_SRC_ALPHA,
				D3D11_BLEND_INV_SRC_ALPHA,
				D3D11_BLEND_OP_ADD,
				D3D11_BLEND_SRC_ALPHA,
				D3D11_BLEND_INV_SRC_ALPHA,
				D3D11_BLEND_OP_ADD,
				D3D11_COLOR_WRITE_ENABLE_ALL,
			},
		};
		if (FAILED(m_pDevice->CreateBlendState(&bsDesc, &m_pBsAlphaBlend)))
		{
			assert(false);
			return false;
		}

		// Set up commonly used samplers

		D3D11_SAMPLER_DESC sampDesc =
		{
			D3D11_FILTER_MIN_MAG_MIP_POINT,
			D3D11_TEXTURE_ADDRESS_CLAMP,
			D3D11_TEXTURE_ADDRESS_CLAMP,
			D3D11_TEXTURE_ADDRESS_CLAMP,
			0.0f,
			1,
			D3D11_COMPARISON_FUNC(0),
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			0.0f,
			FLT_MAX,
		};
		if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSsPointClamp)))
		{
			assert(false);
			return false;
		}

		sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSsBilinearClamp)))
		{
			assert(false);
			return false;
		}
	
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSsTrilinearRepeat)))
		{
			assert(false);
			return false;
		}
	
		sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		sampDesc.MaxAnisotropy = 16;
		if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSsTrilinearRepeatAniso)))
		{
			assert(false);
			return false;
		}

		// PCF shadow comparison filter, with border color set to 1.0 so areas outside
		// the shadow map will be unshadowed
		sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		sampDesc.MaxAnisotropy = 1;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		sampDesc.BorderColor[0] = 1.0f;
		sampDesc.BorderColor[1] = 1.0f;
		sampDesc.BorderColor[2] = 1.0f;
		sampDesc.BorderColor[3] = 1.0f;
		if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSsPCF)))
		{
			assert(false);
			return false;
		}

		return true;
	}

	void D3D11Window::Shutdown()
	{
		LOG("Shutting down");

		if (m_hWnd)
		{
			DestroyWindow(m_hWnd);
		}
	}

	int D3D11Window::MainLoop(int nShowCmd)
	{
		// Show the window.  This sends the initial WM_SIZE message which results in
		// calling OnRender(); we don't want to do this until all initialization 
		// (including subclass init) is done, so it's here instead of in Init().
		ShowWindow(m_hWnd, nShowCmd);

		LOG("Main loop started");

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

			// Render a new frame
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
		LOG("Window resized to %d x %d", width, height);

		m_width = width;
		m_height = height;

		// Have to release old render target views before swap chain can be resized
		m_pRtvSRGB.release();
		m_pRtvRaw.release();
		m_pDsv.release();
		m_pSrvDepth.release();

		// Resize the swap chain to fit the window again
		if (!m_pSwapChain)
		{
			assert(false);
			return;
		}
		if (FAILED(m_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0)))
		{
			assert(false);
			return;
		}

		{
			// Retrieve the back buffer
			comptr<ID3D11Texture2D> pTex;
			if (FAILED(m_pSwapChain->GetBuffer(0, IID_ID3D11Texture2D, (void **)&pTex)))
			{
				assert(false);
				return;
			}

			// Create render target views in sRGB and raw formats
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = 
			{
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				D3D11_RTV_DIMENSION_TEXTURE2D,
			};
			if (FAILED(m_pDevice->CreateRenderTargetView(pTex, &rtvDesc, &m_pRtvSRGB)))
			{
				assert(false);
				return;
			}
			rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			if (FAILED(m_pDevice->CreateRenderTargetView(pTex, &rtvDesc, &m_pRtvRaw)))
			{
				assert(false);
				return;
			}
		}

		{
			// Create depth buffer and its views

			comptr<ID3D11Texture2D> pTexDepth;
			D3D11_TEXTURE2D_DESC texDesc =
			{
				width, height, 1, 1,
				DXGI_FORMAT_R32_TYPELESS,
				{ 1, 0 },
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
			};
			if (FAILED(m_pDevice->CreateTexture2D(&texDesc, nullptr, &pTexDepth)))
			{
				assert(false);
				return;
			}

			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc =
			{
				DXGI_FORMAT_D32_FLOAT,
				D3D11_DSV_DIMENSION_TEXTURE2D,
			};
			if (FAILED(m_pDevice->CreateDepthStencilView(pTexDepth, &dsvDesc, &m_pDsv)))
			{
				assert(false);
				return;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc =
			{
				DXGI_FORMAT_R32_FLOAT,
				D3D11_SRV_DIMENSION_TEXTURE2D,
			};
			srvDesc.Texture2D.MipLevels = 1;
			if (FAILED(m_pDevice->CreateShaderResourceView(pTexDepth, &srvDesc, &m_pSrvDepth)))
			{
				assert(false);
				return;
			}
		}

		// Automatically re-render after changing size
		OnRender();
	}
}
