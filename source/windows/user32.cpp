/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include <Windows.h>

extern "C" ATOM WINAPI HookRegisterClassA(const WNDCLASSA *lpWndClass)
{
	assert(lpWndClass != nullptr);

	WNDCLASSA wndclass = *lpWndClass;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		LOG(INFO) << "Redirecting " << "RegisterClassA" << '(' << "lpWndClass = " << lpWndClass << " { \"" << wndclass.lpszClassName << "\", style = " << std::hex << wndclass.style << std::dec << " }" << ')' << " ...";

		// Need to add 'CS_OWNDC' flag to windows that will be used with OpenGL, so that they consistently use the same device context handle and therefore the lookup code in 'wglSwapBuffers' is able to find a runtime
		// But it must not be added to classes which are used with GDI for widgets, as that may break their drawing due to the assumption that it would be possible to query multiple independent device contexts for a window handle
		// As such the heuristic chosen here is to assume that only windows with the 'CS_VREDRAW' or 'CS_HREDRAW' flags are potentially used with OpenGL, and to ignore those with extra bytes requested, as that is typically done for widgets
		if (0 == (wndclass.style & (CS_OWNDC | CS_CLASSDC)) &&
			(wndclass.style & (CS_VREDRAW | CS_HREDRAW) || wndclass.hIcon != nullptr) && wndclass.cbWndExtra == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to \"" << wndclass.lpszClassName << "\".";

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
		LOG(INFO) << "Redirecting " << "RegisterClassW" << '(' << "lpWndClass = " << lpWndClass << " { \"" << wndclass.lpszClassName << "\", style = " << std::hex << wndclass.style << std::dec << " }" << ')' << " ...";

		if (0 == (wndclass.style & (CS_OWNDC | CS_CLASSDC)) &&
			(wndclass.style & (CS_VREDRAW | CS_HREDRAW) || wndclass.hIcon != nullptr) && wndclass.cbWndExtra == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to \"" << wndclass.lpszClassName << "\".";

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
		LOG(INFO) << "Redirecting " << "RegisterClassExA" << '(' << "lpWndClassEx = " << lpWndClassEx << " { \"" << wndclass.lpszClassName << "\", style = " << std::hex << wndclass.style << std::dec << " }" << ')' << " ...";

		if (0 == (wndclass.style & (CS_OWNDC | CS_CLASSDC)) &&
			(wndclass.style & (CS_VREDRAW | CS_HREDRAW) || wndclass.hIcon != nullptr) && wndclass.cbWndExtra == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to \"" << wndclass.lpszClassName << "\".";

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
		LOG(INFO) << "Redirecting " << "RegisterClassExW" << '(' << "lpWndClassEx = " << lpWndClassEx << " { \"" << wndclass.lpszClassName << "\", style = " << std::hex << wndclass.style << std::dec << " }" << ')' << " ...";

		if (0 == (wndclass.style & (CS_OWNDC | CS_CLASSDC)) &&
			(wndclass.style & (CS_VREDRAW | CS_HREDRAW) || wndclass.hIcon != nullptr) && wndclass.cbWndExtra == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to \"" << wndclass.lpszClassName << "\".";

			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(HookRegisterClassExW)(&wndclass);
}
