#include "log.hpp"
#include "hook_manager.hpp"
#include "input.hpp"

#include <assert.h>
#include <Windows.h>

bool g_block_cursor_reset = false;
POINT g_last_cursor_position = { };

HOOK_EXPORT ATOM WINAPI HookRegisterClassA(CONST WNDCLASSA *lpWndClass)
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
HOOK_EXPORT ATOM WINAPI HookRegisterClassW(CONST WNDCLASSW *lpWndClass)
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
HOOK_EXPORT ATOM WINAPI HookRegisterClassExA(CONST WNDCLASSEXA *lpWndClassEx)
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
HOOK_EXPORT ATOM WINAPI HookRegisterClassExW(CONST WNDCLASSEXW *lpWndClassEx)
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

		LOG(TRACE) << "> Dumping device registration at index " << i << ":";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | Parameter                               | Value                                   |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | UsagePage                               | " << std::setw(39) << std::hex << device.usUsagePage << std::dec << " |";
		LOG(TRACE) << "  | Usage                                   | " << std::setw(39) << std::hex << device.usUsage << std::dec << " |";
		LOG(TRACE) << "  | Flags                                   | " << std::setw(39) << std::hex << device.dwFlags << std::dec << " |";
		LOG(TRACE) << "  | TargetWindow                            | " << std::setw(39) << device.hwndTarget << " |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

		if (device.usUsagePage != 1 || (device.dwFlags & RIDEV_NOLEGACY) == 0 || device.hwndTarget == nullptr)
		{
			continue;
		}

		reshade::input::register_window_with_raw_input(device.hwndTarget);
	}

	if (!reshade::hooks::call(&HookRegisterRawInputDevices)(pRawInputDevices, uiNumDevices, cbSize))
	{
		LOG(WARNING) << "> 'RegisterRawInputDevices' failed with error code " << GetLastError() << "!";

		return FALSE;
	}

	return TRUE;
}

HOOK_EXPORT LRESULT WINAPI HookDispatchMessageA(const MSG *lpmsg)
{
	assert(lpmsg != nullptr);

	if (lpmsg->hwnd != nullptr && reshade::input::handle_window_message(lpmsg))
	{
		return 0;
	}

	static const auto trampoline = reshade::hooks::call(&HookDispatchMessageA);

	return trampoline(lpmsg);
}
HOOK_EXPORT LRESULT WINAPI HookDispatchMessageW(const MSG *lpmsg)
{
	return HookDispatchMessageA(lpmsg);
}

HOOK_EXPORT BOOL WINAPI HookGetCursorPos(LPPOINT lpPoint)
{
	if (g_block_cursor_reset)
	{
		assert(lpPoint != nullptr);

		*lpPoint = g_last_cursor_position;

		return TRUE;
	}

	static const auto trampoline = reshade::hooks::call(&HookGetCursorPos);

	return trampoline(lpPoint);
}
HOOK_EXPORT BOOL WINAPI HookSetCursorPos(int X, int Y)
{
	if (g_block_cursor_reset)
	{
		g_last_cursor_position.x = X;
		g_last_cursor_position.y = Y;

		return TRUE;
	}

	static const auto trampoline = reshade::hooks::call(&HookSetCursorPos);

	return trampoline(X, Y);
}
