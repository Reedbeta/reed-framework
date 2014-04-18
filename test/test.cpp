#include <util.h>
#include <d3d11-window.h>

using namespace util;
using namespace Framework;

class TestWindow : public D3D11Window
{
public:
	typedef D3D11Window super;

	virtual LRESULT MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_KEYUP:
			if (wParam == VK_ESCAPE)
				Shutdown();
			return 0;

		default:
			return super::MsgProc(hWnd, message, wParam, lParam);
		}
	}

	virtual void OnRender()
	{
		m_pCtx->ClearRenderTargetView(m_pRtvRaw, makefloat4(0.286f, 0.600f, 0.871f, 1.0f));
		m_pSwapChain->Present(1, 0);
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	TestWindow w;
	if (!w.Init("TestWindow", "Test", hInstance))
	{
		w.Shutdown();
		return 1;
	}

	return w.MainLoop(nCmdShow);

	(void)hPrevInstance;
	(void)lpCmdLine;
}
