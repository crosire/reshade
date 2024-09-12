/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include <utf8/unchecked.h>
#include <Windows.h>

extern "C" ATOM WINAPI HookRegisterClassA(const WNDCLASSA *lpWndClass)
{
	assert(lpWndClass != nullptr);

	WNDCLASSA wndclass = *lpWndClass;

	// Only care about window classes registered by the application itself
	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		reshade::log::message(reshade::log::level::info, "Redirecting RegisterClassA(lpWndClass = %p { \"%s\", style = %#x }) ...", lpWndClass, wndclass.lpszClassName, wndclass.style);

		// Need to add 'CS_OWNDC' flag to windows that will be used with OpenGL, so that they consistently use the same device context handle and therefore the lookup code in 'wglSwapBuffers' is able to find a swap chain
		// But it must not be added to classes which are used with GDI for widgets, as that may break their drawing due to the assumption that it would be possible to query multiple independent device contexts for a window handle
		// The heuristic here assumes that only windows with the 'CS_VREDRAW' or 'CS_HREDRAW' flags are likely to be used with OpenGL, and ignores those with extra bytes requested, as that is typically done for widgets
		if (0 == (wndclass.style & (CS_OWNDC | CS_CLASSDC)) &&
			(wndclass.style & (CS_VREDRAW | CS_HREDRAW) || wndclass.hIcon != nullptr) && wndclass.cbWndExtra == 0)
		{
			reshade::log::message(reshade::log::level::info, "> Adding 'CS_OWNDC' window class style flag to \"%s\".", wndclass.lpszClassName);

			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(HookRegisterClassA)(&wndclass);
}
extern "C" ATOM WINAPI HookRegisterClassW(const WNDCLASSW *lpWndClass)
{
	assert(lpWndClass != nullptr);

	WNDCLASSW wndclass = *lpWndClass;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		std::string class_name;
		utf8::unchecked::utf16to8(wndclass.lpszClassName, wndclass.lpszClassName + std::wcslen(wndclass.lpszClassName), std::back_inserter(class_name));

		reshade::log::message(reshade::log::level::info, "Redirecting RegisterClassW(lpWndClass = %p { \"%s\", style = %#x }) ...", lpWndClass, class_name.c_str(), wndclass.style);

		if (0 == (wndclass.style & (CS_OWNDC | CS_CLASSDC)) &&
			(wndclass.style & (CS_VREDRAW | CS_HREDRAW) || wndclass.hIcon != nullptr) && wndclass.cbWndExtra == 0)
		{
			reshade::log::message(reshade::log::level::info, "> Adding 'CS_OWNDC' window class style flag to \"%s\".", class_name.c_str());

			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(HookRegisterClassW)(&wndclass);
}
extern "C" ATOM WINAPI HookRegisterClassExA(const WNDCLASSEXA *lpWndClassEx)
{
	assert(lpWndClassEx != nullptr);

	WNDCLASSEXA wndclass = *lpWndClassEx;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		reshade::log::message(reshade::log::level::info, "Redirecting RegisterClassExA(lpWndClassEx = %p { \"%s\", style = %#x }) ...", lpWndClassEx, wndclass.lpszClassName, wndclass.style);

		if (0 == (wndclass.style & (CS_OWNDC | CS_CLASSDC)) &&
			(wndclass.style & (CS_VREDRAW | CS_HREDRAW) || wndclass.hIcon != nullptr) && wndclass.cbWndExtra == 0)
		{
			reshade::log::message(reshade::log::level::info, "> Adding 'CS_OWNDC' window class style flag to \"%s\".", wndclass.lpszClassName);

			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(HookRegisterClassExA)(&wndclass);
}
extern "C" ATOM WINAPI HookRegisterClassExW(const WNDCLASSEXW *lpWndClassEx)
{
	assert(lpWndClassEx != nullptr);

	WNDCLASSEXW wndclass = *lpWndClassEx;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		std::string class_name;
		utf8::unchecked::utf16to8(wndclass.lpszClassName, wndclass.lpszClassName + std::wcslen(wndclass.lpszClassName), std::back_inserter(class_name));

		reshade::log::message(reshade::log::level::info, "Redirecting RegisterClassExW(lpWndClassEx = %p { \"%s\", style = %#x }) ...", lpWndClassEx, class_name.c_str(), wndclass.style);

		if (0 == (wndclass.style & (CS_OWNDC | CS_CLASSDC)) &&
			(wndclass.style & (CS_VREDRAW | CS_HREDRAW) || wndclass.hIcon != nullptr) && wndclass.cbWndExtra == 0)
		{
			reshade::log::message(reshade::log::level::info, "> Adding 'CS_OWNDC' window class style flag to \"%s\".", class_name.c_str());

			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(HookRegisterClassExW)(&wndclass);
}
