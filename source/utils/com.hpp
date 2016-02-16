#pragma once

#include <Unknwn.h>

template <typename T>
inline void safe_release(T *&object)
{
	if (object != nullptr)
	{
		object->Release();
		object = nullptr;
	}
}
inline ULONG refcount(IUnknown *object)
{
	return object->AddRef(), object->Release();
}
