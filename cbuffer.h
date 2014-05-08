#pragma once

namespace Framework
{
	// Wrapper for constant buffers
	template <typename T>
	class CB
	{
	public:
		void	Init(ID3D11Device * pDevice);
		void	Update(ID3D11DeviceContext * pCtx, const T * pData);
		void	Bind(ID3D11DeviceContext * pCtx, uint slot);

		comptr<ID3D11Buffer>	m_pBuf;
	};

	// Inline template implementation

	template <typename T>
	inline void CB<T>::Init(ID3D11Device * pDevice)
	{
		D3D11_BUFFER_DESC bufDesc =
		{
			((sizeof(T) + 15) / 16) * 16,	// Round up to next 16 bytes
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_CONSTANT_BUFFER,
			D3D11_CPU_ACCESS_WRITE,
		};

		CHECK_D3D(pDevice->CreateBuffer(&bufDesc, nullptr, &m_pBuf));
	}

	template <typename T>
	inline void CB<T>::Update(ID3D11DeviceContext * pCtx, const T * pData)
	{
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		CHECK_D3D_WARN(pCtx->Map(m_pBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy(mapped.pData, pData, sizeof(T));
		pCtx->Unmap(m_pBuf, 0);
	}

	template <typename T>
	inline void CB<T>::Bind(ID3D11DeviceContext * pCtx, uint slot)
	{
		pCtx->VSSetConstantBuffers(slot, 1, &m_pBuf);
		pCtx->PSSetConstantBuffers(slot, 1, &m_pBuf);
	}
}
