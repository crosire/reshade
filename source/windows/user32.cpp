/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "input.hpp"
#include <assert.h>
#include <Windows.h>

std::string upperCase(std::string input) {
	for (std::string::iterator it = input.begin(); it != input.end(); ++it)
		*it = toupper((unsigned char)*it);
	return input;
}

HOOK_EXPORT ATOM WINAPI HookRegisterClassA(const WNDCLASSA *lpWndClass)
{
	assert(lpWndClass != nullptr);

	auto wndclass = *lpWndClass;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		LOG(INFO) << "Redirecting RegisterClassA" << '(' << lpWndClass << " { " << wndclass.lpszClassName << " }" << ')' << " ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(HookRegisterClassA)(&wndclass);
}
HOOK_EXPORT ATOM WINAPI HookRegisterClassW(const WNDCLASSW *lpWndClass)
{
	assert(lpWndClass != nullptr);

	auto wndclass = *lpWndClass;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{		
		LOG(INFO) << "Redirecting RegisterClassW" << '(' << lpWndClass << " { " << wndclass.lpszClassName << " }" << ')' << " ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(HookRegisterClassW)(&wndclass);
}
HOOK_EXPORT ATOM WINAPI HookRegisterClassExA(const WNDCLASSEXA *lpWndClassEx)
{
	assert(lpWndClassEx != nullptr);

	auto wndclass = *lpWndClassEx;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		LOG(INFO) << "Redirecting RegisterClassExA" << '(' << lpWndClassEx << " { " << wndclass.lpszClassName << " }" << ')' << " ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(HookRegisterClassExA)(&wndclass);
}
HOOK_EXPORT ATOM WINAPI HookRegisterClassExW(const WNDCLASSEXW *lpWndClassEx)
{
	assert(lpWndClassEx != nullptr);

	auto wndclass = *lpWndClassEx;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		LOG(INFO) << "Redirecting RegisterClassExW" << '(' << lpWndClassEx << " { " << wndclass.lpszClassName << " }" << ')' << " ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(HookRegisterClassExW)(&wndclass);
}
