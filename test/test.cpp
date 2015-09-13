#include <framework.h>
#include <AntTweakBar.h>
#include <xinput.h>
#include "shader-slots.h"

// Shader bytecode generated by build process
#include "world_vs.h"
#include "simple_ps.h"
#include "simple_alphatest_ps.h"
#include "shadow_alphatest_ps.h"
#include "tonemap_ps.h"

using namespace util;
using namespace Framework;



// Globals

float3 g_vecDirectionalLight = normalize(makefloat3(1.0f, 10.0f, 1.5f));
rgb g_rgbDirectionalLight = makergb(1.1f, 1.0f, 0.7f);
rgb g_rgbSky = makergb(0.37f, 0.52f, 1.0f);

float g_normalOffsetShadow = 1e-5f;		// meters
float g_shadowSharpening = 5.0f;

bool g_useTonemapping = true;
float g_exposure = 1.0f;

bool g_debugKey = false;
float g_debugSlider0 = 0.0f;
float g_debugSlider1 = 0.0f;
float g_debugSlider2 = 0.0f;
float g_debugSlider3 = 0.0f;



// Constant buffers

struct CBFrame								// matches cbuffer CBFrame in shader-common.h
{
	float4x4	m_matWorldToClip;
	float4x4	m_matWorldToUvzwShadow;
	float3x4	m_matWorldToUvzShadowNormal;	// actually float3x3, but constant buffer packing rules...
	point3		m_posCamera;
	float		m_padding0;

	float3		m_vecDirectionalLight;
	float		m_padding1;

	rgb			m_rgbDirectionalLight;
	float		m_padding2;

	float2		m_dimsShadowMap;
	float		m_normalOffsetShadow;
	float		m_shadowSharpening;

	float		m_exposure;					// Exposure multiplier
};

struct CBDebug								// matches cbuffer CBDebug in shader-common.h
{
	float		m_debugKey;					// Mapped to spacebar - 0 if up, 1 if down
	float		m_debugSlider0;				// Mapped to debug sliders in UI
	float		m_debugSlider1;				// ...
	float		m_debugSlider2;				// ...
	float		m_debugSlider3;				// ...
};



// Window class

class TestWindow : public D3D11Window
{
public:
	typedef D3D11Window super;

						TestWindow();

	bool				Init(HINSTANCE hInstance);
	virtual void		Shutdown() override;
	virtual LRESULT		MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;
	virtual void		OnResize(int2_arg dimsNew) override;
	virtual void		OnRender() override;
	void				ResetCamera();
	void				RenderScene();
	void				RenderShadowMap();

	// Sponza assets
	Mesh								m_meshSponza;
	MaterialLib							m_mtlLibSponza;
	TextureLib							m_texLibSponza;

	// Render targets
	RenderTarget						m_rtSceneMSAA;
	DepthStencilTarget					m_dstSceneMSAA;
	ShadowMap							m_shmp;

	// Shaders
	comptr<ID3D11VertexShader>			m_pVsWorld;
	comptr<ID3D11PixelShader>			m_pPsSimple;
	comptr<ID3D11PixelShader>			m_pPsSimpleAlphaTest;
	comptr<ID3D11PixelShader>			m_pPsShadowAlphaTest;
	comptr<ID3D11PixelShader>			m_pPsTonemap;

	// Other stuff
	comptr<ID3D11InputLayout>			m_pInputLayout;
	CB<CBFrame>							m_cbFrame;
	CB<CBDebug>							m_cbDebug;
	Texture2D							m_tex1x1White;
	FPSCamera							m_camera;
	Timer								m_timer;
};

// TestWindow implementation

TestWindow::TestWindow()
{
	// Disable framework's automatic depth buffer, since we'll create our own
	m_hasDepthBuffer = false;
}

bool TestWindow::Init(HINSTANCE hInstance)
{
	super::Init("TestWindow", "Test", hInstance);

	// Ensure the asset pack is up to date
	static const AssetCompileInfo s_assets[] =
	{
		{ "crytek-sponza/sponza.obj",								ACK_OBJMesh, },
		{ "crytek-sponza/sponza.mtl",								ACK_OBJMtlLib, },
		{ "crytek-sponza/textures/background.tga",					ACK_TextureWithMips, },
		{ "crytek-sponza/textures/backgroundbgr.tga",				ACK_TextureWithMips, },
		{ "crytek-sponza/textures/background_bump.png",				ACK_TextureWithMips, },
		{ "crytek-sponza/textures/chain_texture.tga",				ACK_TextureWithMips, },
		{ "crytek-sponza/textures/chain_texture_bump.png",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/chain_texture_mask.png",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/gi_flag.tga",						ACK_TextureWithMips, },
		{ "crytek-sponza/textures/lion.tga",						ACK_TextureWithMips, },
		{ "crytek-sponza/textures/lion2_bump.png",					ACK_TextureWithMips, },
		{ "crytek-sponza/textures/lion_bump.png",					ACK_TextureWithMips, },
		{ "crytek-sponza/textures/spnza_bricks_a_bump.png",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/spnza_bricks_a_diff.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/spnza_bricks_a_spec.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_arch_bump.png",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_arch_diff.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_arch_spec.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_ceiling_a_diff.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_ceiling_a_spec.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_column_a_bump.png",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_column_a_diff.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_column_a_spec.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_column_b_bump.png",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_column_b_diff.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_column_b_spec.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_column_c_bump.png",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_column_c_diff.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_column_c_spec.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_curtain_blue_diff.tga",	ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_curtain_diff.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_curtain_green_diff.tga",	ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_details_diff.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_details_spec.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_fabric_blue_diff.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_fabric_diff.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_fabric_green_diff.tga",	ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_fabric_spec.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_flagpole_diff.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_flagpole_spec.tga",		ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_floor_a_diff.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_floor_a_spec.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_roof_diff.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_thorn_bump.png",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_thorn_diff.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_thorn_mask.png",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/sponza_thorn_spec.tga",			ACK_TextureWithMips, },
		{ "crytek-sponza/textures/vase_bump.png",					ACK_TextureWithMips, },
		{ "crytek-sponza/textures/vase_dif.tga",					ACK_TextureWithMips, },
		{ "crytek-sponza/textures/vase_hanging.tga",				ACK_TextureWithMips, },
		{ "crytek-sponza/textures/vase_plant.tga",					ACK_TextureWithMips, },
		{ "crytek-sponza/textures/vase_plant_mask.png",				ACK_TextureWithMips, },
		{ "crytek-sponza/textures/vase_plant_spec.tga",				ACK_TextureWithMips, },
		{ "crytek-sponza/textures/vase_round.tga",					ACK_TextureWithMips, },
		{ "crytek-sponza/textures/vase_round_bump.png",				ACK_TextureWithMips, },
		{ "crytek-sponza/textures/vase_round_spec.tga",				ACK_TextureWithMips, },
	};
	comptr<AssetPack> pPack = new AssetPack;
	if (!LoadAssetPackOrCompileIfOutOfDate("crytek-sponza-assets.zip", s_assets, dim(s_assets), pPack))
	{
		ERR("Couldn't load or compile Sponza asset pack");
		return false;
	}

	// Load assets
	if (!LoadTextureLibFromAssetPack(pPack, s_assets, dim(s_assets), &m_texLibSponza))
	{
		ERR("Couldn't load Sponza texture library");
		return false;
	}
	if (!LoadMaterialLibFromAssetPack(pPack, "crytek-sponza/sponza.mtl", &m_texLibSponza, &m_mtlLibSponza))
	{
		ERR("Couldn't load Sponza material library");
		return false;
	}
	if (!LoadMeshFromAssetPack(pPack, "crytek-sponza/sponza.obj", &m_mtlLibSponza, &m_meshSponza))
	{
		ERR("Couldn't load Sponza mesh");
		return false;
	}

	// Hardcode a list of alpha-tested materials, for now
	static const char * s_aMtlAlphaTest[] =
	{
		"leaf",
		"material__57",
		"chain",
	};
	for (int i = 0; i < dim(s_aMtlAlphaTest); ++i)
	{
		if (Material * pMtl = m_mtlLibSponza.Lookup(s_aMtlAlphaTest[i]))
			pMtl->m_alphaTest = true;
	}

	// Upload all assets to GPU
	m_meshSponza.UploadToGPU(m_pDevice);
	m_texLibSponza.UploadAllToGPU(m_pDevice);

	// Init shadow map
	m_shmp.Init(m_pDevice, makeint2(4096));

	// Load shaders
	CHECK_D3D(m_pDevice->CreateVertexShader(world_vs_bytecode, dim(world_vs_bytecode), nullptr, &m_pVsWorld));
	CHECK_D3D(m_pDevice->CreatePixelShader(simple_ps_bytecode, dim(simple_ps_bytecode), nullptr, &m_pPsSimple));
	CHECK_D3D(m_pDevice->CreatePixelShader(simple_alphatest_ps_bytecode, dim(simple_alphatest_ps_bytecode), nullptr, &m_pPsSimpleAlphaTest));
	CHECK_D3D(m_pDevice->CreatePixelShader(shadow_alphatest_ps_bytecode, dim(shadow_alphatest_ps_bytecode), nullptr, &m_pPsShadowAlphaTest));
	CHECK_D3D(m_pDevice->CreatePixelShader(tonemap_ps_bytecode, dim(tonemap_ps_bytecode), nullptr, &m_pPsTonemap));

	// Initialize the input layout, and validate it against all the vertex shaders

	D3D11_INPUT_ELEMENT_DESC aInputDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, UINT(offsetof(Vertex, m_pos)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, UINT(offsetof(Vertex, m_normal)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, UINT(offsetof(Vertex, m_uv)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	CHECK_D3D(m_pDevice->CreateInputLayout(
							aInputDescs, dim(aInputDescs),
							world_vs_bytecode, dim(world_vs_bytecode),
							&m_pInputLayout));

	// Init constant buffers
	m_cbFrame.Init(m_pDevice);
	m_cbDebug.Init(m_pDevice);

	// Init default textures
	CreateTexture1x1(m_pDevice, makergba(1.0f), &m_tex1x1White);

	// Init the camera
	m_camera.m_moveSpeed = 3.0f;
	m_camera.m_mbuttonActivate = MBUTTON_Left;
	ResetCamera();

	// Init AntTweakBar
	CHECK_ERR(TwInit(TW_DIRECT3D11, m_pDevice));

	// Automatically use the biggest font size
	TwDefine("GLOBAL fontsize=3 fontresizable=false");

	// Create bar for FPS display
	TwBar * pTwBarFPS = TwNewBar("FPS");
	TwDefine("FPS position='15 15' size='225 80' valueswidth=75 refresh=0.5");
	TwAddVarCB(
			pTwBarFPS, "FPS", TW_TYPE_FLOAT,
			nullptr,
			[](void * value, void * timestep) { 
				*(float *)value = 1.0f / *(float *)timestep;
			},
			&m_timer.m_timestep,
			"precision=1");
	TwAddVarCB(
			pTwBarFPS, "Frame time (ms)", TW_TYPE_FLOAT,
			nullptr,
			[](void * value, void * timestep) { 
				*(float *)value = 1000.0f * *(float *)timestep;
			},
			&m_timer.m_timestep,
			"precision=2");

	// Create bar for debug sliders
	TwBar * pTwBarDebug = TwNewBar("Debug");
	TwDefine("Debug position='15 110' size='225 115' valueswidth=75");
	TwAddVarRW(pTwBarDebug, "g_debugSlider0", TW_TYPE_FLOAT, &g_debugSlider0, "min=0.0 step=0.01 precision=2");
	TwAddVarRW(pTwBarDebug, "g_debugSlider1", TW_TYPE_FLOAT, &g_debugSlider1, "min=0.0 step=0.01 precision=2");
	TwAddVarRW(pTwBarDebug, "g_debugSlider2", TW_TYPE_FLOAT, &g_debugSlider2, "min=0.0 step=0.01 precision=2");
	TwAddVarRW(pTwBarDebug, "g_debugSlider3", TW_TYPE_FLOAT, &g_debugSlider3, "min=0.0 step=0.01 precision=2");

	// Create bar for rendering options
	TwBar * pTwBarRendering = TwNewBar("Rendering");
	TwDefine("Rendering position='15 240' size='275 355' valueswidth=130");
	TwAddVarRW(pTwBarRendering, "Light direction", TW_TYPE_DIR3F, &g_vecDirectionalLight, nullptr);
	TwAddVarRW(pTwBarRendering, "Light color", TW_TYPE_COLOR3F, &g_rgbDirectionalLight, nullptr);
	TwAddVarRW(pTwBarRendering, "Sky color", TW_TYPE_COLOR3F, &g_rgbSky, nullptr);
	TwAddVarRW(pTwBarRendering, "Normal Offset", TW_TYPE_FLOAT, &g_normalOffsetShadow, "min=0.0 max=1e-4 step=1e-6 precision=6 group=Shadow");
	TwAddVarRW(pTwBarRendering, "Sharpening", TW_TYPE_FLOAT, &g_shadowSharpening, "min=0.01 max=10.0 step=0.01 precision=2 group=Shadow");
	TwAddVarRW(pTwBarRendering, "Tonemapping", TW_TYPE_BOOLCPP, &g_useTonemapping, nullptr);
	TwAddVarRW(pTwBarRendering, "Exposure", TW_TYPE_FLOAT, &g_exposure, "min=0.01 max=5.0 step=0.01 precision=2");

	// Create bar for camera position and orientation
	TwBar * pTwBarCamera = TwNewBar("Camera");
	TwDefine("Camera position='255 15' size='195 180' valueswidth=75 refresh=0.5");
	TwAddVarRO(pTwBarCamera, "Camera X", TW_TYPE_FLOAT, &m_camera.m_pos.x, "precision=3");
	TwAddVarRO(pTwBarCamera, "Camera Y", TW_TYPE_FLOAT, &m_camera.m_pos.y, "precision=3");
	TwAddVarRO(pTwBarCamera, "Camera Z", TW_TYPE_FLOAT, &m_camera.m_pos.z, "precision=3");
	TwAddVarRO(pTwBarCamera, "Yaw", TW_TYPE_FLOAT, &m_camera.m_yaw, "precision=3");
	TwAddVarRO(pTwBarCamera, "Pitch", TW_TYPE_FLOAT, &m_camera.m_pitch, "precision=3");
	auto lambdaNegate = [](void * outValue, void * inValue) { *(float *)outValue = -*(float *)inValue; };
	TwAddVarCB(pTwBarCamera, "Look X", TW_TYPE_FLOAT, nullptr, lambdaNegate, &m_camera.m_viewToWorld.m_linear[2].x, "precision=3");
	TwAddVarCB(pTwBarCamera, "Look Y", TW_TYPE_FLOAT, nullptr, lambdaNegate, &m_camera.m_viewToWorld.m_linear[2].y, "precision=3");
	TwAddVarCB(pTwBarCamera, "Look Z", TW_TYPE_FLOAT, nullptr, lambdaNegate, &m_camera.m_viewToWorld.m_linear[2].z, "precision=3");

	return true;
}

void TestWindow::Shutdown()
{
	TwTerminate();

	m_meshSponza.Reset();
	m_mtlLibSponza.Reset();
	m_texLibSponza.Reset();

	m_rtSceneMSAA.Reset();
	m_dstSceneMSAA.Reset();
	m_shmp.Reset();

	m_pVsWorld.release();
	m_pPsSimple.release();
	m_pPsSimpleAlphaTest.release();
	m_pPsShadowAlphaTest.release();
	m_pPsTonemap.release();

	m_pInputLayout.release();
	m_cbFrame.Reset();
	m_cbDebug.Reset();
	m_tex1x1White.Reset();

	super::Shutdown();
}

LRESULT TestWindow::MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Give AntTweakBar and the camera a crack at the message
	if (TwEventWin(hWnd, message, wParam, lParam))
		return 0;
	if (m_camera.HandleWindowsMessage(message, wParam, lParam))
		return 0;

	switch (message)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_HOME:
			ResetCamera();
			break;

		case VK_ESCAPE:
			Shutdown();
			break;
		}
		return 0;

	default:
		return super::MsgProc(hWnd, message, wParam, lParam);
	}
}

void TestWindow::ResetCamera()
{
	m_camera.LookAt(
				makepoint3(-8.7f, 6.8f, 0.0f),
				makepoint3(0.0f, 5.0f, 0.0f));
}

void TestWindow::OnResize(int2_arg dimsNew)
{
	super::OnResize(dimsNew);

	// Recreate MSAA render targets for the new size
	m_rtSceneMSAA.Reset();
	m_dstSceneMSAA.Reset();
	m_rtSceneMSAA.Init(m_pDevice, dimsNew, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4);
	m_dstSceneMSAA.Init(m_pDevice, dimsNew, DXGI_FORMAT_D32_FLOAT, 4);

	// Update projection matrix for new aspect ratio
	m_camera.SetProjection(1.0f, float(dimsNew.x) / float(dimsNew.y), 0.1f, 1000.0f);
}

void TestWindow::OnRender()
{
	m_timer.OnFrameStart();
	m_camera.Update(m_timer.m_timestep);

	m_pCtx->ClearState();
	m_pCtx->IASetInputLayout(m_pInputLayout);
	m_pCtx->OMSetDepthStencilState(m_pDssDepthTest, 0);

	// Set up debug parameters constant buffer

	XINPUT_STATE controllerState = {};
	{
		static bool controllerPresent = true;
		if (controllerPresent && XInputGetState(0, &controllerState) != ERROR_SUCCESS)
			controllerPresent = false;
	}
	// !!!UNDONE: move keyboard tracking into an input system that respects focus, etc.
	g_debugKey = (GetAsyncKeyState(' ') || (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_A));

	CBDebug cbDebug =
	{
		// !!!UNDONE: move keyboard tracking into an input system that respects focus, etc.
		g_debugKey ? 1.0f : 0.0f,
		g_debugSlider0,
		g_debugSlider1,
		g_debugSlider2,
		g_debugSlider3,
	};
	m_cbDebug.Update(m_pCtx, &cbDebug);
	m_cbDebug.Bind(m_pCtx, CB_DEBUG);

	RenderShadowMap();
	RenderScene();

	// Draw AntTweakBar UI
	BindRawBackBuffer(m_pCtx);
	CHECK_WARN(TwDraw());

	CHECK_D3D(m_pSwapChain->Present(1, 0));
}

void TestWindow::RenderScene()
{
	// Crytek Sponza is authored in centimeters; convert to meters
	float sceneScale = 0.01f;
	float4x4 matSceneScale = diagonal(makefloat4(sceneScale, sceneScale, sceneScale, 1.0f));

	// Set up constant buffer for rendering to shadow map
	CBFrame cbFrame =
	{
		matSceneScale * m_camera.m_worldToClip,
		matSceneScale * m_shmp.m_matWorldToUvzw,
		makefloat3x4(m_shmp.m_matWorldToUvzNormal),
		m_camera.m_pos,
		0,	// padding
		g_vecDirectionalLight,
		0,	// padding
		g_rgbDirectionalLight,
		0,	// padding
		makefloat2(m_shmp.m_dst.m_dims),
		g_normalOffsetShadow,
		g_shadowSharpening,
		g_exposure,
	};
	m_cbFrame.Update(m_pCtx, &cbFrame);
	m_cbFrame.Bind(m_pCtx, CB_FRAME);

	m_pCtx->ClearRenderTargetView(m_rtSceneMSAA.m_pRtv, makergba(toLinear(g_rgbSky), 1.0f));
	m_pCtx->ClearDepthStencilView(m_dstSceneMSAA.m_pDsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
	BindRenderTargets(m_pCtx, &m_rtSceneMSAA, &m_dstSceneMSAA);

	m_pCtx->VSSetShader(m_pVsWorld, nullptr, 0);

	m_pCtx->PSSetShaderResources(TEX_SHADOW, 1, &m_shmp.m_dst.m_pSrvDepth);
	m_pCtx->PSSetSamplers(SAMP_DEFAULT, 1, &m_pSsTrilinearRepeatAniso);
	m_pCtx->PSSetSamplers(SAMP_SHADOW, 1, &m_pSsPCF);

	// Draw the individual material ranges of the mesh

	// Non-alpha-tested materials
	m_pCtx->PSSetShader(m_pPsSimple, nullptr, 0);
	m_pCtx->RSSetState(m_pRsDefault);
	for (int i = 0, c = int(m_meshSponza.m_mtlRanges.size()); i < c; ++i)
	{
		Material * pMtl = m_meshSponza.m_mtlRanges[i].m_pMtl;
		ASSERT_ERR(pMtl);

		if (pMtl->m_alphaTest)
			continue;

		ID3D11ShaderResourceView * pSrv = m_tex1x1White.m_pSrv;
		if (Texture2D * pTex = pMtl->m_pTexDiffuseColor)
			pSrv = pTex->m_pSrv;

		m_pCtx->PSSetShaderResources(TEX_DIFFUSE, 1, &pSrv);
		m_meshSponza.DrawMtlRange(m_pCtx, i);
	}

	// Alpha-tested materials
	m_pCtx->PSSetShader(m_pPsSimpleAlphaTest, nullptr, 0);
	m_pCtx->RSSetState(m_pRsDoubleSided);
	for (int i = 0, c = int(m_meshSponza.m_mtlRanges.size()); i < c; ++i)
	{
		Material * pMtl = m_meshSponza.m_mtlRanges[i].m_pMtl;
		ASSERT_ERR(pMtl);

		if (!pMtl->m_alphaTest)
			continue;

		ID3D11ShaderResourceView * pSrv = m_tex1x1White.m_pSrv;
		if (Texture2D * pTex = pMtl->m_pTexDiffuseColor)
			pSrv = pTex->m_pSrv;

		m_pCtx->PSSetShaderResources(TEX_DIFFUSE, 1, &pSrv);
		m_meshSponza.DrawMtlRange(m_pCtx, i);
	}

	// Resolve from the MSAA buffer to the back buffer
	if (g_useTonemapping)
	{
		// Custom resolve + tonemapping pass
		BindSRGBBackBuffer(m_pCtx);
		m_pCtx->OMSetDepthStencilState(m_pDssNoDepthTest, 0);
		m_pCtx->PSSetShader(m_pPsTonemap, nullptr, 0);
		m_pCtx->PSSetShaderResources(0, 1, &m_rtSceneMSAA.m_pSrv);
		DrawFullscreenPass(m_pCtx);
	}
	else
	{
		// Standard box filter resolve
		m_pCtx->ResolveSubresource(m_pTexBackBuffer, 0, m_rtSceneMSAA.m_pTex, 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	}
}

void TestWindow::RenderShadowMap()
{
	// Crytek Sponza is authored in centimeters; convert to meters
	float sceneScale = 0.01f;
	float4x4 matSceneScale = diagonal(makefloat4(sceneScale, sceneScale, sceneScale, 1.0f));

	// Calculate shadow map matrices
	m_shmp.m_vecLight = g_vecDirectionalLight;
	m_shmp.m_boundsScene = makebox3(m_meshSponza.m_bounds.m_mins * sceneScale, m_meshSponza.m_bounds.m_maxs * sceneScale);
	m_shmp.UpdateMatrix();

	m_pCtx->IASetInputLayout(m_pInputLayout);
	m_pCtx->OMSetDepthStencilState(m_pDssDepthTest, 0);

	// Set up constant buffer for rendering to shadow map
	CBFrame cbFrame =
	{
		matSceneScale * m_shmp.m_matWorldToClip,
	};
	m_cbFrame.Update(m_pCtx, &cbFrame);
	m_cbFrame.Bind(m_pCtx, CB_FRAME);

	m_pCtx->ClearDepthStencilView(m_shmp.m_dst.m_pDsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
	m_shmp.Bind(m_pCtx);

	m_pCtx->VSSetShader(m_pVsWorld, nullptr, 0);
	m_pCtx->PSSetSamplers(SAMP_DEFAULT, 1, &m_pSsTrilinearRepeatAniso);

	// Draw the individual material ranges of the mesh

	// Non-alpha-tested materials
	m_pCtx->PSSetShader(nullptr, nullptr, 0);
	m_pCtx->RSSetState(m_pRsDefault);
	for (int i = 0, c = int(m_meshSponza.m_mtlRanges.size()); i < c; ++i)
	{
		Material * pMtl = m_meshSponza.m_mtlRanges[i].m_pMtl;
		ASSERT_ERR(pMtl);

		if (pMtl->m_alphaTest)
			continue;

		m_meshSponza.DrawMtlRange(m_pCtx, i);
	}

	// Alpha-tested materials
	m_pCtx->PSSetShader(m_pPsShadowAlphaTest, nullptr, 0);
	m_pCtx->RSSetState(m_pRsDoubleSided);
	for (int i = 0, c = int(m_meshSponza.m_mtlRanges.size()); i < c; ++i)
	{
		Material * pMtl = m_meshSponza.m_mtlRanges[i].m_pMtl;
		ASSERT_ERR(pMtl);

		if (!pMtl->m_alphaTest)
			continue;

		ID3D11ShaderResourceView * pSrvDiffuse = m_tex1x1White.m_pSrv;
		if (Texture2D * pTex = pMtl->m_pTexDiffuseColor)
			pSrvDiffuse = pTex->m_pSrv;

		m_pCtx->PSSetShaderResources(TEX_DIFFUSE, 1, &pSrvDiffuse);
		m_meshSponza.DrawMtlRange(m_pCtx, i);
	}
}



// Get the whole shebang going

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;

	TestWindow w;
	if (!w.Init(hInstance))
	{
		w.Shutdown();
		return 1;
	}

	w.MainLoop(SW_SHOWMAXIMIZED);
	return 0;
}
