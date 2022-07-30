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

#define ID3D12Resource_GetDevice call_vtable<7, HRESULT, ID3D12Resource, REFIID, void **>

#define ID3D12Resource_Map call_vtable<8, HRESULT, ID3D12Resource, UINT, const D3D12_RANGE *, void **>
#define ID3D12Resource_Unmap call_vtable<9, void, ID3D12Resource, UINT, const D3D12_RANGE *>

#else

#define ID3D12Resource_GetDevice(p, a, b) (p)->GetDevice(a, b)

#define ID3D12Resource_Map(p, a, b, c) (p)->Map(a, b, c)
#define ID3D12Resource_Unmap(p, a, b) (p)->Unmap(a, b)

#endif
