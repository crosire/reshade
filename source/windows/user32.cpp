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
		LOG(INFO) << "Redirecting '" << "RegisterClassA" << "(" << lpWndClass << ")' ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

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
		LOG(INFO) << "Redirecting '" << "RegisterClassW" << "(" << lpWndClass << ")' ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

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
		LOG(INFO) << "Redirecting '" << "RegisterClassExA" << "(" << lpWndClassEx << ")' ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

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
		LOG(INFO) << "Redirecting '" << "RegisterClassExW" << "(" << lpWndClassEx << ")' ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

			wndclass.style |= CS_OWNDC;
		}
	}

	return reshade::hooks::call(&HookRegisterClassExW)(&wndclass);
}

HOOK_EXPORT BOOL WINAPI HookRegisterRawInputDevices(PCRAWINPUTDEVICE pRawInputDevices, UINT uiNumDevices, UINT cbSize)
{
	LOG(INFO) << "Redirecting '" << "RegisterRawInputDevices" << "(" << pRawInputDevices << ", " << uiNumDevices << ", " << cbSize << ")' ...";

	for (UINT i = 0; i < uiNumDevices; ++i)
	{
		const auto &device = pRawInputDevices[i];

		LOG(INFO) << "> Dumping device registration at index " << i << ":";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(INFO) << "  | Parameter                               | Value                                   |";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(INFO) << "  | UsagePage                               | " << std::setw(39) << std::hex << device.usUsagePage << std::dec << " |";
		LOG(INFO) << "  | Usage                                   | " << std::setw(39) << std::hex << device.usUsage << std::dec << " |";
		LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << device.dwFlags << std::dec << " |";
		LOG(INFO) << "  | TargetWindow                            | " << std::setw(39) << device.hwndTarget << " |";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

		if (device.usUsagePage != 1 || device.hwndTarget == nullptr)
		{
			continue;
		}

		reshade::input::register_window_with_raw_input(device.hwndTarget, device.usUsage == 0x06 && (device.dwFlags & RIDEV_NOLEGACY) != 0, device.usUsage == 0x02 && (device.dwFlags & RIDEV_NOLEGACY) != 0);
	}

	if (!reshade::hooks::call(&HookRegisterRawInputDevices)(pRawInputDevices, uiNumDevices, cbSize))
	{
		LOG(WARNING) << "> 'RegisterRawInputDevices' failed with error code " << GetLastError() << "!";

		return FALSE;
	}

	return TRUE;
}

HOOK_EXPORT BOOL WINAPI HookGetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
	static const auto trampoline = reshade::hooks::call(&HookGetMessageA);

	if (!trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax))
	{
		return FALSE;
	}

	assert(lpMsg != nullptr);

	if (lpMsg->hwnd != nullptr && reshade::input::handle_window_message(lpMsg))
	{
		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}

	return TRUE;
}
HOOK_EXPORT BOOL WINAPI HookGetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
	static const auto trampoline = reshade::hooks::call(&HookGetMessageW);

	if (!trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax))
	{
		return FALSE;
	}

	assert(lpMsg != nullptr);

	if (lpMsg->hwnd != nullptr && reshade::input::handle_window_message(lpMsg))
	{
		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}

	return TRUE;
}
HOOK_EXPORT BOOL WINAPI HookPeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
	static const auto trampoline = reshade::hooks::call(&HookPeekMessageA);

	if (!trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg))
	{
		return FALSE;
	}

	assert(lpMsg != nullptr);

	if (lpMsg->hwnd != nullptr && (wRemoveMsg & PM_REMOVE) != 0 && reshade::input::handle_window_message(lpMsg))
	{
		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}

	return TRUE;
}
HOOK_EXPORT BOOL WINAPI HookPeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
	static const auto trampoline = reshade::hooks::call(&HookPeekMessageW);

	if (!trampoline(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg))
	{
		return FALSE;
	}

	assert(lpMsg != nullptr);

	if (lpMsg->hwnd != nullptr && (wRemoveMsg & PM_REMOVE) != 0 && reshade::input::handle_window_message(lpMsg))
	{
		// Change message so it is ignored by the recipient window
		lpMsg->message = WM_NULL;
	}

	return TRUE;
}

POINT last_cursor_position = { };
std::shared_ptr<reshade::input> last_input;

HOOK_EXPORT BOOL WINAPI HookSetCursorPosition(int X, int Y)
{
	const HWND hwnd = GetActiveWindow();

	if (hwnd != nullptr)
	{
		last_input = reshade::input::register_window(hwnd);

		if (last_input->is_blocking_mouse_input())
		{
			last_cursor_position.x = X;
			last_cursor_position.y = Y;

			return TRUE;
		}
	}

	static const auto trampoline = reshade::hooks::call(&HookSetCursorPosition);

	return trampoline(X, Y);
}
HOOK_EXPORT BOOL WINAPI HookGetCursorPosition(LPPOINT lpPoint)
{
	const HWND hwnd = GetActiveWindow();

	if (hwnd != nullptr)
	{
		last_input = reshade::input::register_window(hwnd);

		if (last_input->is_blocking_mouse_input())
		{
			assert(lpPoint != nullptr);

			*lpPoint = last_cursor_position;

			return TRUE;
		}
	}

	static const auto trampoline = reshade::hooks::call(&HookGetCursorPosition);

	return trampoline(lpPoint);
}
