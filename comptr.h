#pragma once

namespace Framework
{
	// Auto-releasing wrapper for COM pointers
	template <typename T>
	struct comptr
	{
		T * p;

		comptr(): p(nullptr) {}
		comptr(T * other): p(other)
			{ if (p) p->AddRef(); }
		comptr(const comptr<T> & other): p(other.p)
			{ if (p) p->AddRef(); }
		comptr(comptr<T> && other): p(other.p)
			{ other.p = nullptr; }

		void release()
			{ if (p) { p->Release(); p = nullptr; } }
		~comptr()
			{ release(); }

		comptr<T> & operator = (T * other)
			{ release(); p = other; if (p) p->AddRef(); return *this; }
		comptr<T> & operator = (const comptr<T> & other)
			{ release(); p = other.p; if (p) p->AddRef(); return *this; }
		comptr<T> & operator = (comptr<T> && other)
			{ release(); p = other.p; other.p = nullptr; return *this; }

		T ** operator & () { return &p; }
		T * operator * () { return p; }
		T * operator -> () { return p; }
		operator T * () { return p; }
	};



	// Reference counting mixin functionality that's interface-compatible with COM
	struct RefCount
	{
		int		m_cRef;

		RefCount(): m_cRef(0) {}
		virtual ~RefCount() { ASSERT_ERR(m_cRef == 0); }

		void AddRef()
		{
			++m_cRef;
		}
		
		void Release()
		{
			--m_cRef;
			if (m_cRef == 0)
				delete this;
		}
	};
}
