#include "Log.hpp"
#include "HookManager.hpp"

#include <Windows.h>

// -----------------------------------------------------------------------------------------------------

EXPORT ATOM WINAPI RegisterClassA(CONST WNDCLASSA *lpWndClass)
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

	return ReShade::Hooks::Call(&RegisterClassA)(&wndclass);
}
EXPORT ATOM WINAPI RegisterClassW(CONST WNDCLASSW *lpWndClass)
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

	return ReShade::Hooks::Call(&RegisterClassW)(&wndclass);
}
EXPORT ATOM WINAPI RegisterClassExA(CONST WNDCLASSEXA *lpWndClassEx)
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

	return ReShade::Hooks::Call(&RegisterClassExA)(&wndclass);
}
EXPORT ATOM WINAPI RegisterClassExW(CONST WNDCLASSEXW *lpWndClassEx)
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

	return ReShade::Hooks::Call(&RegisterClassExW)(&wndclass);
}

EXPORT BOOL WINAPI RegisterRawInputDevices(PCRAWINPUTDEVICE pRawInputDevices, UINT uiNumDevices, UINT cbSize)
{
	LOG(INFO) << "Redirecting '" << "RegisterRawInputDevices" << "(" << pRawInputDevices << ", " << uiNumDevices << ", " << cbSize << ")' ...";

	RAWINPUTDEVICE *const devices = new RAWINPUTDEVICE[uiNumDevices];

	for (UINT i = 0; i < uiNumDevices; ++i)
	{
		devices[i] = pRawInputDevices[i];

		LOG(TRACE) << "> Dumping device registration at index " << i << ":";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | Parameter                               | Value                                   |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+" << std::left;
		LOG(TRACE) << "  | UsagePage                               | " << std::setw(39) << std::showbase << std::hex << devices[i].usUsagePage << std::dec << std::noshowbase << " |";
		LOG(TRACE) << "  | Usage                                   | " << std::setw(39) << std::showbase << std::hex << devices[i].usUsage << std::dec << std::noshowbase << " |";
		LOG(TRACE) << "  | Flags                                   | " << std::setw(39) << std::showbase << std::hex << devices[i].dwFlags << std::dec << std::noshowbase << " |";
		LOG(TRACE) << "  | TargetWindow                            | " << std::setw(39) << devices[i].hwndTarget << " |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+" << std::internal;

		if (devices[i].usUsagePage != 1 || (devices[i].dwFlags & RIDEV_NOLEGACY) == 0)
		{
			continue;
		}

		switch (devices[i].usUsage)
		{
			case 0x02:
				LOG(WARNING) << "> Removing 'RIDEV_NOLEGACY' flag from mouse device targeting window " << devices[i].hwndTarget << ".";
				break;
			case 0x06:
				LOG(WARNING) << "> Removing 'RIDEV_NOLEGACY' flag from keyboard device targeting window " << devices[i].hwndTarget << ".";
				break;
			default:
				continue;
		}

		if (devices[i].hwndTarget == nullptr)
		{
			devices[i].dwFlags &= ~(RIDEV_NOLEGACY | RIDEV_APPKEYS);
		}
		else
		{
			devices[i].dwFlags &= ~(RIDEV_NOLEGACY | RIDEV_NOHOTKEYS | RIDEV_APPKEYS);
		}
	}

	const BOOL res = ReShade::Hooks::Call(&RegisterRawInputDevices)(devices, uiNumDevices, cbSize);

	delete[] devices;

	if (!res)
	{
		LOG(WARNING) << "> 'RegisterRawInputDevices' failed with '" << GetLastError() << "'!";

		return FALSE;
	}

	return TRUE;
}