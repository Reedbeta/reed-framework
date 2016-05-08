#include "framework.h"

namespace Framework
{
	// RenderTarget implementation

	RenderTarget::RenderTarget()
	:	m_dims(0),
		m_sampleCount(0),
		m_format(DXGI_FORMAT_UNKNOWN)
	{
	}

	void RenderTarget::Init(
		ID3D11Device * pDevice,
		int2 dims,
		DXGI_FORMAT format,
		int sampleCount, /* = 1 */
		int flags /* = RTFLAG_Default */)
	{
		ASSERT_ERR(pDevice);

		// Always map the format to its typeless version, if possible;
		// enables views of other formats to be created if desired
		DXGI_FORMAT formatTex = FindTypelessFormat(format);
		if (formatTex == DXGI_FORMAT_UNKNOWN)
			formatTex = format;

		D3D11_TEXTURE2D_DESC texDesc =
		{
			UINT(dims.x), UINT(dims.y), 1, 1,
			formatTex,
			{ UINT(sampleCount), 0 },
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
			0, 0,
		};
		if (flags & RTFLAG_EnableUAV)
		{
			texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		CHECK_D3D(pDevice->CreateTexture2D(&texDesc, nullptr, &m_pTex));

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc =
		{
			format,
			(sampleCount > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D,
		};
		CHECK_D3D(pDevice->CreateRenderTargetView(m_pTex, &rtvDesc, &m_pRtv));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc =
		{
			format,
			(sampleCount > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D,
		};
		if (sampleCount == 1)
			srvDesc.Texture2D.MipLevels = 1;
		CHECK_D3D(pDevice->CreateShaderResourceView(m_pTex, &srvDesc, &m_pSrv));

		if (flags & RTFLAG_EnableUAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = { format, D3D11_UAV_DIMENSION_TEXTURE2D, };
			CHECK_D3D(pDevice->CreateUnorderedAccessView(m_pTex, &uavDesc, &m_pUav));
		}

		m_dims = dims;
		m_sampleCount = sampleCount;
		m_format = format;
	}

	void RenderTarget::Reset()
	{
		m_pTex.release();
		m_pRtv.release();
		m_pSrv.release();
		m_pUav.release();
		m_dims = int2(0);
		m_sampleCount = 0;
		m_format = DXGI_FORMAT_UNKNOWN;
	}

	void RenderTarget::Bind(ID3D11DeviceContext * pCtx)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(1, &m_pRtv, nullptr);
		D3D11_VIEWPORT d3dViewport = { 0.0f, 0.0f, float(m_dims.x), float(m_dims.y), 0.0f, 1.0f, };
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void RenderTarget::Bind(ID3D11DeviceContext * pCtx, box2 viewport)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(1, &m_pRtv, nullptr);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.mins.x, viewport.mins.y,
			viewport.maxs.x - viewport.mins.x, viewport.maxs.y - viewport.mins.y,
			0.0f, 1.0f,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void RenderTarget::Bind(ID3D11DeviceContext * pCtx, box3 viewport)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(1, &m_pRtv, nullptr);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.mins.x, viewport.mins.y,
			viewport.maxs.x - viewport.mins.x, viewport.maxs.y - viewport.mins.y,
			viewport.mins.z, viewport.maxs.z,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void RenderTarget::Readback(
		ID3D11DeviceContext * pCtx,
		void * pDataOut)
	{
		ASSERT_ERR(m_pTex);
		ASSERT_ERR_MSG(m_sampleCount == 1, "D3D11 doesn't support readback of multisampled render targets");
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pDataOut);

		comptr<ID3D11Device> pDevice;
		pCtx->GetDevice(&pDevice);

		// Create a staging resource
		D3D11_TEXTURE2D_DESC texDesc =
		{
			UINT(m_dims.x), UINT(m_dims.y), 1, 1,
			m_format,
			{ 1, 0 },
			D3D11_USAGE_STAGING,
			0,
			D3D11_CPU_ACCESS_READ,
			0,
		};
		comptr<ID3D11Texture2D> pTexStaging;
		pDevice->CreateTexture2D(&texDesc, nullptr, &pTexStaging);

		// Copy the data to the staging resource
		pCtx->CopyResource(pTexStaging, m_pTex);

		// Map the staging resource
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		CHECK_D3D(pCtx->Map(pTexStaging, 0, D3D11_MAP_READ, 0, &mapped));

		// Copy the data out row by row, in case the pitch is different
		int rowSize = m_dims.x * BitsPerPixel(m_format) / 8;
		ASSERT_ERR(mapped.RowPitch >= UINT(rowSize));
		for (int y = 0; y < m_dims.y; ++y)
		{
			memcpy(
				offsetPtr(pDataOut, y * rowSize),
				offsetPtr(mapped.pData, y * mapped.RowPitch),
				rowSize);
		}

		pCtx->Unmap(pTexStaging, 0);
	}



	// DepthStencilTarget implementation

	struct DepthStencilFormats
	{
		DXGI_FORMAT		m_formatTypeless;
		DXGI_FORMAT		m_formatDsv;
		DXGI_FORMAT		m_formatSrvDepth;
		DXGI_FORMAT		m_formatSrvStencil;
	};

	static const DepthStencilFormats s_depthStencilFormats[] =
	{
		// 32-bit float depth + 8-bit stencil
		{
			DXGI_FORMAT_R32G8X24_TYPELESS,
			DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
			DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
			DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,
		},
		// 32-bit float depth, no stencil
		{
			DXGI_FORMAT_R32_TYPELESS,
			DXGI_FORMAT_D32_FLOAT,
			DXGI_FORMAT_R32_FLOAT,
			DXGI_FORMAT_UNKNOWN,
		},
		// 24-bit fixed-point depth + 8-bit stencil
		{
			DXGI_FORMAT_R24G8_TYPELESS,
			DXGI_FORMAT_D24_UNORM_S8_UINT,
			DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
			DXGI_FORMAT_X24_TYPELESS_G8_UINT,
		},
		// 16-bit fixed-point depth, no stencil
		{
			DXGI_FORMAT_R16_TYPELESS,
			DXGI_FORMAT_D16_UNORM,
			DXGI_FORMAT_R16_UNORM,
			DXGI_FORMAT_UNKNOWN,
		},
	};

	DepthStencilTarget::DepthStencilTarget()
	:	m_dims(0),
		m_sampleCount(0),
		m_formatDsv(DXGI_FORMAT_UNKNOWN),
		m_formatSrvDepth(DXGI_FORMAT_UNKNOWN),
		m_formatSrvStencil(DXGI_FORMAT_UNKNOWN)
	{
	}

	void DepthStencilTarget::Init(
		ID3D11Device * pDevice,
		int2 dims,
		DXGI_FORMAT format,
		int sampleCount, /* = 1 */
		int flags /* = DSFLAG_Default */)
	{
		ASSERT_ERR(pDevice);

		// Check that the format matches one of our known depth-stencil formats
		DepthStencilFormats formats = {};
		for (int i = 0; i < dim(s_depthStencilFormats); ++i)
		{
			if (s_depthStencilFormats[i].m_formatDsv == format)
				formats = s_depthStencilFormats[i];
		}
		ASSERT_ERR_MSG(
			formats.m_formatDsv == format,
			"Depth-stencil format must be one of those listed in s_depthStencilFormats; found %s instead",
			NameOfFormat(format));

		D3D11_TEXTURE2D_DESC texDesc =
		{
			UINT(dims.x), UINT(dims.y), 1, 1,
			formats.m_formatTypeless,
			{ UINT(sampleCount), 0 },
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
			0, 0,
		};
		if (flags & RTFLAG_EnableUAV)
		{
			texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		CHECK_D3D(pDevice->CreateTexture2D(&texDesc, nullptr, &m_pTex));

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc =
		{
			formats.m_formatDsv,
			(sampleCount > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D,
		};
		CHECK_D3D(pDevice->CreateDepthStencilView(m_pTex, &dsvDesc, &m_pDsv));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc =
		{
			formats.m_formatSrvDepth,
			(sampleCount > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D,
		};
		if (sampleCount == 1)
			srvDesc.Texture2D.MipLevels = 1;
		CHECK_D3D(pDevice->CreateShaderResourceView(m_pTex, &srvDesc, &m_pSrvDepth));

		if (formats.m_formatSrvStencil != DXGI_FORMAT_UNKNOWN)
		{
			srvDesc.Format = formats.m_formatSrvStencil;
			CHECK_D3D(pDevice->CreateShaderResourceView(m_pTex, &srvDesc, &m_pSrvStencil));
		}

		if (flags & DSFLAG_EnableUAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = { formats.m_formatSrvDepth, D3D11_UAV_DIMENSION_TEXTURE2D, };
			CHECK_D3D(pDevice->CreateUnorderedAccessView(m_pTex, &uavDesc, &m_pUavDepth));

			if (formats.m_formatSrvStencil != DXGI_FORMAT_UNKNOWN)
			{
				uavDesc.Format = formats.m_formatSrvStencil;
				CHECK_D3D(pDevice->CreateUnorderedAccessView(m_pTex, &uavDesc, &m_pUavStencil));
			}
		}

		m_dims = dims;
		m_sampleCount = sampleCount;
		m_formatDsv = format;
		m_formatSrvDepth = formats.m_formatSrvDepth;
		m_formatSrvStencil = formats.m_formatSrvStencil;
	}

	void DepthStencilTarget::Reset()
	{
		m_pTex.release();
		m_pDsv.release();
		m_pSrvDepth.release();
		m_pSrvStencil.release();
		m_pUavDepth.release();
		m_pUavStencil.release();
		m_dims = int2(0);
		m_sampleCount = 0;
		m_formatDsv = DXGI_FORMAT_UNKNOWN;
		m_formatSrvDepth = DXGI_FORMAT_UNKNOWN;
		m_formatSrvStencil = DXGI_FORMAT_UNKNOWN;
	}

	void DepthStencilTarget::Bind(ID3D11DeviceContext * pCtx)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(0, nullptr, m_pDsv);
		D3D11_VIEWPORT d3dViewport = { 0.0f, 0.0f, float(m_dims.x), float(m_dims.y), 0.0f, 1.0f, };
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void DepthStencilTarget::Bind(ID3D11DeviceContext * pCtx, box2 viewport)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(0, nullptr, m_pDsv);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.mins.x, viewport.mins.y,
			viewport.maxs.x - viewport.mins.x, viewport.maxs.y - viewport.mins.y,
			0.0f, 1.0f,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void DepthStencilTarget::Bind(ID3D11DeviceContext * pCtx, box3 viewport)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(0, nullptr, m_pDsv);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.mins.x, viewport.mins.y,
			viewport.maxs.x - viewport.mins.x, viewport.maxs.y - viewport.mins.y,
			viewport.mins.z, viewport.maxs.z,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void DepthStencilTarget::Readback(
		ID3D11DeviceContext * pCtx,
		void * pDataOut)
	{
		ASSERT_ERR(m_pTex);
		ASSERT_ERR_MSG(m_sampleCount == 1, "D3D11 doesn't support readback of multisampled render targets");
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pDataOut);

		comptr<ID3D11Device> pDevice;
		pCtx->GetDevice(&pDevice);

		// Create a staging resource
		D3D11_TEXTURE2D_DESC texDesc =
		{
			UINT(m_dims.x), UINT(m_dims.y), 1, 1,
			m_formatDsv,
			{ 1, 0 },
			D3D11_USAGE_STAGING,
			0,
			D3D11_CPU_ACCESS_READ,
			0,
		};
		comptr<ID3D11Texture2D> pTexStaging;
		pDevice->CreateTexture2D(&texDesc, nullptr, &pTexStaging);

		// Copy the data to the staging resource
		pCtx->CopyResource(pTexStaging, m_pTex);

		// Map the staging resource
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		CHECK_D3D(pCtx->Map(pTexStaging, 0, D3D11_MAP_READ, 0, &mapped));

		// Copy the data out row by row, in case the pitch is different
		int rowSize = m_dims.x * BitsPerPixel(m_formatDsv) / 8;
		ASSERT_ERR(mapped.RowPitch >= UINT(rowSize));
		for (int y = 0; y < m_dims.y; ++y)
		{
			memcpy(
				offsetPtr(pDataOut, y * rowSize),
				offsetPtr(mapped.pData, y * mapped.RowPitch),
				rowSize);
		}

		pCtx->Unmap(pTexStaging, 0);
	}



	// Utility functions for binding multiple render targets

	void BindRenderTargets(ID3D11DeviceContext * pCtx, RenderTarget * pRt, DepthStencilTarget * pDst)
	{
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pRt);
		ASSERT_ERR(pRt->m_pRtv);
		if (pDst)
			ASSERT_ERR(all(pRt->m_dims == pDst->m_dims));

		pCtx->OMSetRenderTargets(1, &pRt->m_pRtv, pDst ? pDst->m_pDsv : nullptr);
		D3D11_VIEWPORT d3dViewport = { 0.0f, 0.0f, float(pRt->m_dims.x), float(pRt->m_dims.y), 0.0f, 1.0f, };
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void BindRenderTargets(ID3D11DeviceContext * pCtx, RenderTarget * pRt, DepthStencilTarget * pDst, box2 viewport)
	{
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pRt);
		ASSERT_ERR(pRt->m_pRtv);
		if (pDst)
			ASSERT_ERR(all(pRt->m_dims == pDst->m_dims));

		pCtx->OMSetRenderTargets(1, &pRt->m_pRtv, pDst ? pDst->m_pDsv : nullptr);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.mins.x, viewport.mins.y,
			viewport.maxs.x - viewport.mins.x, viewport.maxs.y - viewport.mins.y,
			0.0f, 1.0f,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void BindRenderTargets(ID3D11DeviceContext * pCtx, RenderTarget * pRt, DepthStencilTarget * pDst, box3 viewport)
	{
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pRt);
		ASSERT_ERR(pRt->m_pRtv);
		if (pDst)
			ASSERT_ERR(all(pRt->m_dims == pDst->m_dims));

		pCtx->OMSetRenderTargets(1, &pRt->m_pRtv, pDst ? pDst->m_pDsv : nullptr);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.mins.x, viewport.mins.y,
			viewport.maxs.x - viewport.mins.x, viewport.maxs.y - viewport.mins.y,
			viewport.mins.z, viewport.maxs.z,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}



	// Helper functions for saving out screenshots of render targets

	bool WriteRenderTargetToBMP(
		ID3D11DeviceContext * pCtx,
		RenderTarget * pRt,
		const char * path)
	{
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pRt);
		ASSERT_ERR(all(pRt->m_dims > 0));
		ASSERT_ERR(path);

		// Currently the texture must be in RGBA8 format and can't be multisampled
		ASSERT_ERR(pRt->m_format == DXGI_FORMAT_R8G8B8A8_UNORM || pRt->m_format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		ASSERT_ERR(pRt->m_sampleCount == 1);

		std::vector<byte4> pixels(pRt->m_dims.x * pRt->m_dims.y);
		pRt->Readback(pCtx, &pixels[0]);

		return WriteBMPToFile(&pixels[0], pRt->m_dims, path);
	}
}
