#include "log.hpp"
#include "hook_manager.hpp"
#include "input.hpp"

#include <assert.h>
#include <Windows.h>

#define EXPORT extern "C"

// ---------------------------------------------------------------------------------------------------

EXPORT ATOM WINAPI HookRegisterClassA(CONST WNDCLASSA *lpWndClass)
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
EXPORT ATOM WINAPI HookRegisterClassW(CONST WNDCLASSW *lpWndClass)
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
EXPORT ATOM WINAPI HookRegisterClassExA(CONST WNDCLASSEXA *lpWndClassEx)
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
EXPORT ATOM WINAPI HookRegisterClassExW(CONST WNDCLASSEXW *lpWndClassEx)
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

EXPORT BOOL WINAPI HookRegisterRawInputDevices(PCRAWINPUTDEVICE pRawInputDevices, UINT uiNumDevices, UINT cbSize)
{
	LOG(INFO) << "Redirecting '" << "RegisterRawInputDevices" << "(" << pRawInputDevices << ", " << uiNumDevices << ", " << cbSize << ")' ...";

	for (UINT i = 0; i < uiNumDevices; ++i)
	{
		const auto &device = pRawInputDevices[i];

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

		reshade::input::register_raw_input_device(device);
	}

	if (!reshade::hooks::call(&HookRegisterRawInputDevices)(pRawInputDevices, uiNumDevices, cbSize))
	{
		LOG(WARNING) << "> 'RegisterRawInputDevices' failed with '" << GetLastError() << "'!";

		return FALSE;
	}

	return TRUE;
}
