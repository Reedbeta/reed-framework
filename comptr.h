#pragma once

namespace Framework
{
	// Auto-releasing wrapper for COM pointers
	template <typename T>
	struct comptr
	{
		T * p;

		comptr(): p(nullptr) {}
		comptr(T * other): p(other) {}
		comptr(const comptr<T> & other): p(other.p)
			{ p->AddRef(); }

		void release()
			{ if (p) { p->Release(); p = nullptr; } }
		~comptr()
			{ release(); }

		comptr<T> & operator = (T * other)
			{ release(); p = other; return *this; }
		comptr<T> & operator = (const comptr<T> & other)
			{ release(); p = other.p; p->AddRef(); return *this; }

		T ** operator & () { return &p; }
		T * operator * () { return p; }
		T * operator -> () { return p; }
		operator T * () { return p; }
	};
}
