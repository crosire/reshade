/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON && !RESHADE_ADDON_LITE

#include "hook_manager.hpp"

template <size_t vtable_index, typename R, typename T, typename... Args>
static inline R call_vtable(T *object, Args... args)
{
	const auto vtable_entry = vtable_from_instance(object) + vtable_index;
	const auto func = reshade::hooks::is_hooked(vtable_entry) ?
		reshade::hooks::call<R(STDMETHODCALLTYPE *)(T *, Args...)>(nullptr, vtable_entry) :
		reinterpret_cast<R(STDMETHODCALLTYPE *)(T *, Args...)>(*vtable_entry);
	return func(object, std::forward<Args>(args)...);
}

#define ID3D10Buffer_Map call_vtable<10, HRESULT, ID3D10Buffer, D3D10_MAP, UINT, void **>
#define ID3D10Buffer_Unmap call_vtable<11, HRESULT, ID3D10Buffer>

#define ID3D10Texture1D_Map call_vtable<10, HRESULT, ID3D10Texture1D, UINT, D3D10_MAP, UINT, void **>
#define ID3D10Texture1D_Unmap call_vtable<11, HRESULT, ID3D10Texture1D, UINT>

#define ID3D10Texture2D_Map call_vtable<10, HRESULT, ID3D10Texture2D, UINT, D3D10_MAP, UINT, D3D10_MAPPED_TEXTURE2D *>
#define ID3D10Texture2D_Unmap call_vtable<11, HRESULT, ID3D10Texture2D, UINT>

#define ID3D10Texture3D_Map call_vtable<10, HRESULT, ID3D10Texture3D, UINT, D3D10_MAP, UINT, D3D10_MAPPED_TEXTURE3D *>
#define ID3D10Texture3D_Unmap call_vtable<11, HRESULT, ID3D10Texture3D, UINT>

#else

#define ID3D10Buffer_Map(p, a, b, c) (p)->Map(a, b, c)
#define ID3D10Buffer_Unmap(p) (p)->Unmap()
#define ID3D10Texture1D_Map(p, a, b, c, d) (p)->Map(a, b, c, d)
#define ID3D10Texture1D_Unmap(p, a) (p)->Unmap(a)
#define ID3D10Texture2D_Map(p, a, b, c, d) (p)->Map(a, b, c, d)
#define ID3D10Texture2D_Unmap(p, a) (p)->Unmap(a)
#define ID3D10Texture3D_Map(p, a, b, c, d) (p)->Map(a, b, c, d)
#define ID3D10Texture3D_Unmap(p, a) (p)->Unmap(a)

#endif
