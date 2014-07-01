#include "framework.h"

namespace Framework
{
	RenderTarget::RenderTarget()
	:	m_pTex(),
		m_pRtv(),
		m_pSrv(),
		m_pUav(),
		m_dims(makeint2(0)),
		m_sampleCount(0),
		m_format(DXGI_FORMAT_UNKNOWN)
	{
	}

	void RenderTarget::Init(
		ID3D11Device * pDevice,
		int2_arg dims,
		DXGI_FORMAT format,
		int sampleCount, /* = 1 */
		int flags /* = RTFLAG_Default */)
	{
		ASSERT_ERR(pDevice);

		D3D11_TEXTURE2D_DESC texDesc =
		{
			dims.x, dims.y, 1, 1,
			format,
			{ sampleCount, 0 },
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

	void RenderTarget::Release()
	{
		m_pTex.release();
		m_pRtv.release();
		m_pSrv.release();
		m_pUav.release();
		m_dims = makeint2(0);
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

	void RenderTarget::Bind(ID3D11DeviceContext * pCtx, box2_arg viewport)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(1, &m_pRtv, nullptr);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.m_mins.x, viewport.m_mins.y,
			viewport.diagonal().x, viewport.diagonal().y,
			0.0f, 1.0f,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void RenderTarget::Bind(ID3D11DeviceContext * pCtx, box3_arg viewport)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(1, &m_pRtv, nullptr);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.m_mins.x, viewport.m_mins.y,
			viewport.diagonal().x, viewport.diagonal().y,
			viewport.m_mins.z, viewport.m_maxs.z,
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

		int sizeInBytes = SizeInBytes();

		// Create a staging resource
		D3D11_TEXTURE2D_DESC texDesc =
		{
			m_dims.x, m_dims.y, 1, 1,
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

		// Map the staging resource and copy the data out
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		CHECK_D3D(pCtx->Map(pTexStaging, 0, D3D11_MAP_READ, 0, &mapped));
		ASSERT_ERR(mapped.RowPitch == UINT(m_dims.x * BitsPerPixel(m_format) / 8));
		memcpy(pDataOut, mapped.pData, sizeInBytes);
		pCtx->Unmap(pTexStaging, 0);
	}
}
