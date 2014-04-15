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
		comptr(const comptr<T> & other): p(other.p) { p->AddRef(); }
		~comptr() { release(); }
		void release() { if (p) { p->Release(); p = nullptr; } }
		T ** operator & () { return &p; }
		T * operator * () { return p; }
		T * operator -> () { return p; }
		operator T * () { return p; }
	};
}
