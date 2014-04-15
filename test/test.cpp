#include <util.h>
#include <d3d11-window.h>

class TestWindow : public Framework::D3D11Window
{
public:
	virtual void OnRender()
	{
		// Dummy load
		Sleep(20);
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	TestWindow w;
	if (!w.Init("TestWindow", "Test", hInstance, nCmdShow))
	{
		w.Shutdown();
		return 1;
	}

	return w.MainLoop();

	(void)hPrevInstance;
	(void)lpCmdLine;
}
