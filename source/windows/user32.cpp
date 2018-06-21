/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "input.hpp"
#include <assert.h>
#include <Windows.h>

HOOK_EXPORT ATOM WINAPI HookRegisterClassA(const WNDCLASSA *lpWndClass)
{
	assert(lpWndClass != nullptr);

	auto wndclass = *lpWndClass;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Redirecting '" << "RegisterClassA" << "(" << lpWndClass << ")' ...";
#endif
		if ((wndclass.style & CS_OWNDC) == 0)
		{
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";
#endif
			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(&HookRegisterClassA)(&wndclass);
}
HOOK_EXPORT ATOM WINAPI HookRegisterClassW(const WNDCLASSW *lpWndClass)
{
	assert(lpWndClass != nullptr);

	auto wndclass = *lpWndClass;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Redirecting '" << "RegisterClassW" << "(" << lpWndClass << ")' ...";
#endif
		if ((wndclass.style & CS_OWNDC) == 0)
		{
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";
#endif
			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(&HookRegisterClassW)(&wndclass);
}
HOOK_EXPORT ATOM WINAPI HookRegisterClassExA(const WNDCLASSEXA *lpWndClassEx)
{
	assert(lpWndClassEx != nullptr);

	auto wndclass = *lpWndClassEx;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Redirecting '" << "RegisterClassExA" << "(" << lpWndClassEx << ")' ...";
#endif
		if ((wndclass.style & CS_OWNDC) == 0)
		{
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";
#endif
			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(&HookRegisterClassExA)(&wndclass);
}
HOOK_EXPORT ATOM WINAPI HookRegisterClassExW(const WNDCLASSEXW *lpWndClassEx)
{
	assert(lpWndClassEx != nullptr);

	auto wndclass = *lpWndClassEx;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Redirecting '" << "RegisterClassExW" << "(" << lpWndClassEx << ")' ...";
#endif
		if ((wndclass.style & CS_OWNDC) == 0)
		{
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";
#endif
			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(&HookRegisterClassExW)(&wndclass);
}
