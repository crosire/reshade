/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

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

#define ID3D11Resource_GetDevice call_vtable<3, HRESULT, ID3D11Resource, ID3D11Device **>
