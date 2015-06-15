#include "Log.hpp"
#include "WindowWatcher.hpp"

#include <windowsx.h>

std::unordered_map<HWND, HHOOK> WindowWatcher::sRawInputHooks;
std::vector<std::pair<HWND, WindowWatcher *>> WindowWatcher::sWatchers;

WindowWatcher::WindowWatcher(HWND hwnd) : mWnd(hwnd), mKeys(), mMousePos(), mMouseButtons()
{
	sWatchers.push_back(std::make_pair(hwnd, this));

	this->mHookWindowProc = SetWindowsHookEx(WH_GETMESSAGE, &HookWindowProc, nullptr, GetWindowThreadProcessId(hwnd, nullptr));
}
WindowWatcher::~WindowWatcher()
{
	sWatchers.erase(std::find_if(sWatchers.begin(), sWatchers.end(), [this](const std::pair<HWND, WindowWatcher *> &it) { return it.second == this; }));

	UnhookWindowsHookEx(this->mHookWindowProc);
}

LRESULT CALLBACK WindowWatcher::HookWindowProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < HC_ACTION || wParam != PM_REMOVE)
	{
		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	MSG details = *reinterpret_cast<LPMSG>(lParam);

	if (details.hwnd == nullptr)
	{
		details.hwnd = GetActiveWindow();
	}

	auto it = std::find_if(sWatchers.begin(), sWatchers.end(), [&details](const std::pair<HWND, WindowWatcher *> &it) { return it.first == details.hwnd; });

	if (it == sWatchers.end())
	{
		if (!sWatchers.empty() && sRawInputHooks.find(details.hwnd) != sRawInputHooks.end())
		{
			// Just pick the first window watcher since it is rare to have more than one active window at a time.
			it = sWatchers.begin();
		}
		else
		{
			return CallNextHookEx(nullptr, nCode, wParam, lParam);
		}
	}

	ScreenToClient(it->second->mWnd, &details.pt);

	it->second->mMousePos.x = details.pt.x;
	it->second->mMousePos.y = details.pt.y;

	switch (details.message)
	{
		case WM_INPUT:
			if (GET_RAWINPUT_CODE_WPARAM(details.wParam) == RIM_INPUT)
			{
				RAWINPUT data;
				UINT dataSize = sizeof(RAWINPUT);

				if (GetRawInputData(reinterpret_cast<HRAWINPUT>(details.lParam), RID_INPUT, &data, &dataSize, sizeof(RAWINPUTHEADER)) < 0)
				{
					break;
				}

				switch (data.header.dwType)
				{
					case RIM_TYPEMOUSE:
						if (data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
						{
							it->second->mMouseButtons[0] = 1;
						}
						else if (data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
						{
							it->second->mMouseButtons[0] = 0;
						}
						if (data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
						{
							it->second->mMouseButtons[2] = 1;
						}
						else if (data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
						{
							it->second->mMouseButtons[2] = 0;
						}
						if (data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
						{
							it->second->mMouseButtons[1] = 1;
						}
						else if (data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
						{
							it->second->mMouseButtons[1] = 0;
						}
						break;
					case RIM_TYPEKEYBOARD:
						if (data.data.keyboard.VKey != 0xFF)
						{
							it->second->mKeys[data.data.keyboard.VKey] = (data.data.keyboard.Flags & RI_KEY_BREAK) == 0;
						}
						break;
				}
			}
			break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			it->second->mKeys[details.wParam] = 1;
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			it->second->mKeys[details.wParam] = 0;
			break;
		case WM_LBUTTONDOWN:
			it->second->mMouseButtons[0] = 1;
			break;
		case WM_LBUTTONUP:
			it->second->mMouseButtons[0] = 0;
			break;
		case WM_RBUTTONDOWN:
			it->second->mMouseButtons[2] = 1;
			break;
		case WM_RBUTTONUP:
			it->second->mMouseButtons[2] = 0;
			break;
		case WM_MBUTTONDOWN:
			it->second->mMouseButtons[1] = 1;
			break;
		case WM_MBUTTONUP:
			it->second->mMouseButtons[1] = 0;
			break;
	}

	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void WindowWatcher::RegisterRawInputDevice(const RAWINPUTDEVICE &device)
{
	const auto insert = sRawInputHooks.insert(std::make_pair(device.hwndTarget, nullptr));

	if (insert.second)
	{
		insert.first->second = SetWindowsHookEx(WH_GETMESSAGE, &HookWindowProc, nullptr, GetWindowThreadProcessId(device.hwndTarget, nullptr));
	}
}
void WindowWatcher::UnRegisterRawInputDevices()
{
	for (auto &it : sRawInputHooks)
	{
		UnhookWindowsHookEx(it.second);
	}

	sRawInputHooks.clear();
}

void WindowWatcher::NextFrame()
{
	for (unsigned int i = 0; i < 256; ++i)
	{
		if (this->mKeys[i] == 1)
		{
			this->mKeys[i] = 2;
		}
	}
}