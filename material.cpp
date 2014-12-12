#include "framework.h"

namespace Framework
{
	MaterialLib::MaterialLib()
	{
	}

	Material * MaterialLib::Lookup(const char * name)
	{
		ASSERT_ERR(name);

		auto iter = m_mtls.find(std::string(name));
		if (iter == m_mtls.end())
			return nullptr;

		return &iter->second;
	}

	void MaterialLib::Reset()
	{
		m_pPack.release();
		m_mtls.clear();
	}
}
