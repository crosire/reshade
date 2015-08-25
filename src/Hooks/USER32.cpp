#include "Log.hpp"
#include "HookManager.hpp"
#include "WindowWatcher.hpp"

#include <Windows.h>

#define EXPORT extern "C"

// ---------------------------------------------------------------------------------------------------

EXPORT ATOM WINAPI HookRegisterClassA(CONST WNDCLASSA *lpWndClass)
{
	assert(lpWndClass != nullptr);

	WNDCLASSA wndclass = *lpWndClass;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		LOG(INFO) << "Redirecting '" << "RegisterClassA" << "(" << lpWndClass << ")' ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

			wndclass.style |= CS_OWNDC;
		}
	}

	return ReShade::Hooks::Call(&HookRegisterClassA)(&wndclass);
}
EXPORT ATOM WINAPI HookRegisterClassW(CONST WNDCLASSW *lpWndClass)
{
	assert(lpWndClass != nullptr);

	WNDCLASSW wndclass = *lpWndClass;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		LOG(INFO) << "Redirecting '" << "RegisterClassW" << "(" << lpWndClass << ")' ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

			wndclass.style |= CS_OWNDC;
		}
	}

	return ReShade::Hooks::Call(&HookRegisterClassW)(&wndclass);
}
EXPORT ATOM WINAPI HookRegisterClassExA(CONST WNDCLASSEXA *lpWndClassEx)
{
	assert(lpWndClassEx != nullptr);

	WNDCLASSEXA wndclass = *lpWndClassEx;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		LOG(INFO) << "Redirecting '" << "RegisterClassExA" << "(" << lpWndClassEx << ")' ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

			wndclass.style |= CS_OWNDC;
		}
	}

	return ReShade::Hooks::Call(&HookRegisterClassExA)(&wndclass);
}
EXPORT ATOM WINAPI HookRegisterClassExW(CONST WNDCLASSEXW *lpWndClassEx)
{
	assert(lpWndClassEx != nullptr);

	WNDCLASSEXW wndclass = *lpWndClassEx;

	if (wndclass.hInstance == GetModuleHandle(nullptr))
	{
		LOG(INFO) << "Redirecting '" << "RegisterClassExW" << "(" << lpWndClassEx << ")' ...";

		if ((wndclass.style & CS_OWNDC) == 0)
		{
			LOG(INFO) << "> Adding 'CS_OWNDC' window class style flag to '" << wndclass.lpszClassName << "'.";

			wndclass.style |= CS_OWNDC;
		}
	}

	return ReShade::Hooks::Call(&HookRegisterClassExW)(&wndclass);
}

EXPORT BOOL WINAPI HookRegisterRawInputDevices(PCRAWINPUTDEVICE pRawInputDevices, UINT uiNumDevices, UINT cbSize)
{
	LOG(INFO) << "Redirecting '" << "RegisterRawInputDevices" << "(" << pRawInputDevices << ", " << uiNumDevices << ", " << cbSize << ")' ...";

	for (UINT i = 0; i < uiNumDevices; ++i)
	{
		const RAWINPUTDEVICE &device = pRawInputDevices[i];

		LOG(TRACE) << "> Dumping device registration at index " << i << ":";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | Parameter                               | Value                                   |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | UsagePage                               | " << std::setw(39) << std::showbase << std::hex << device.usUsagePage << std::dec << std::noshowbase << " |";
		LOG(TRACE) << "  | Usage                                   | " << std::setw(39) << std::showbase << std::hex << device.usUsage << std::dec << std::noshowbase << " |";
		LOG(TRACE) << "  | Flags                                   | " << std::setw(39) << std::showbase << std::hex << device.dwFlags << std::dec << std::noshowbase << " |";
		LOG(TRACE) << "  | TargetWindow                            | " << std::setw(39) << device.hwndTarget << " |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

		if (device.usUsagePage != 1 || (device.dwFlags & RIDEV_NOLEGACY) == 0 || device.hwndTarget == nullptr)
		{
			continue;
		}

		ReShade::WindowWatcher::RegisterRawInputDevice(device);
	}

	const BOOL res = ReShade::Hooks::Call(&HookRegisterRawInputDevices)(pRawInputDevices, uiNumDevices, cbSize);

	if (!res)
	{
		LOG(WARNING) << "> 'RegisterRawInputDevices' failed with '" << GetLastError() << "'!";

		return FALSE;
	}

	return TRUE;
}
